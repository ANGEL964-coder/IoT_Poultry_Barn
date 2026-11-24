package com.sdn.poultry.controller;

import com.sdn.poultry.entity.Device;
import com.sdn.poultry.repository.DeviceRepository;
import com.sdn.poultry.security.ClientIpResolver;
import com.sdn.poultry.service.PersistenceService;
import jakarta.servlet.http.HttpServletRequest;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.time.Instant;
import java.util.List;
import java.util.Map;
import java.util.Optional;

@RestController
@RequestMapping("/api/devices")
@CrossOrigin(origins = "*")
public class DeviceController {

    private static final Logger log = LoggerFactory.getLogger(DeviceController.class);

    private final DeviceRepository deviceRepository;
    private final PersistenceService persistenceService;

    public DeviceController(
            DeviceRepository deviceRepository,
            @Autowired(required = false) PersistenceService persistenceService
    ) {
        this.deviceRepository = deviceRepository;
        this.persistenceService = persistenceService;
    }

    /**
     * GET /api/devices - List registered devices (OPERADOR & ADMIN)
     */
    @GetMapping({"", "/"})
    public ResponseEntity<List<Device>> listDevices() {
        List<Device> devices = deviceRepository.findAll();
        return ResponseEntity.ok(devices);
    }

    /**
     * GET /api/devices/{devEui} - Get device details (OPERADOR & ADMIN)
     */
    @GetMapping("/{devEui}")
    public ResponseEntity<?> getDevice(@PathVariable String devEui) {
        String cleanEui = devEui.toLowerCase().trim();
        Optional<Device> devOpt = deviceRepository.findById(cleanEui);
        if (devOpt.isEmpty()) {
            return ResponseEntity.status(HttpStatus.NOT_FOUND).body(Map.of(
                    "error", "Not Found",
                    "message", "Dispositivo no encontrado con DevEUI: " + cleanEui
            ));
        }
        return ResponseEntity.ok(devOpt.get());
    }

    /**
     * POST /api/devices - Register new hardware node (ROLE_ADMIN only)
     */
    @PostMapping({"", "/"})
    public ResponseEntity<?> registerDevice(
            @RequestBody Device device,
            HttpServletRequest request
    ) {
        if (device == null || device.getDevEui() == null || device.getDevEui().isBlank()) {
            return ResponseEntity.badRequest().body(Map.of(
                    "error", "Bad Request",
                    "message", "El campo devEui es obligatorio"
            ));
        }

        String cleanEui = device.getDevEui().toLowerCase().trim();
        device.setDevEui(cleanEui);
        if (device.getCreatedAt() == null) {
            device.setCreatedAt(Instant.now());
        }
        if (device.getLastSeen() == null) {
            device.setLastSeen(Instant.now());
        }
        if (device.getStatus() == null || device.getStatus().isBlank()) {
            device.setStatus("ONLINE");
        }

        Device saved = deviceRepository.save(device);

        String clientIp = ClientIpResolver.resolveClientIp(request);
        if (persistenceService != null) {
            try {
                persistenceService.logAudit(
                        "DEVICE_REGISTERED",
                        "New device registered: " + cleanEui,
                        "Name: " + device.getName() + ", AppId: " + device.getApplicationId(),
                        clientIp
                );
            } catch (Exception e) {
                log.warn("Failed to audit device registration: {}", e.getMessage());
            }
        }

        return ResponseEntity.status(HttpStatus.CREATED).body(saved);
    }

    /**
     * PUT /api/devices/{devEui} - Update device configuration (ROLE_ADMIN only)
     */
    @PutMapping("/{devEui}")
    public ResponseEntity<?> updateDevice(
            @PathVariable String devEui,
            @RequestBody Device updateReq,
            HttpServletRequest request
    ) {
        String cleanEui = devEui.toLowerCase().trim();
        Optional<Device> devOpt = deviceRepository.findById(cleanEui);
        if (devOpt.isEmpty()) {
            return ResponseEntity.status(HttpStatus.NOT_FOUND).body(Map.of(
                    "error", "Not Found",
                    "message", "Dispositivo no encontrado con DevEUI: " + cleanEui
            ));
        }

        Device device = devOpt.get();
        if (updateReq.getName() != null) device.setName(updateReq.getName());
        if (updateReq.getApplicationId() != null) device.setApplicationId(updateReq.getApplicationId());
        if (updateReq.getStatus() != null) device.setStatus(updateReq.getStatus());
        if (updateReq.getHopperCapacityKg() != null) device.setHopperCapacityKg(updateReq.getHopperCapacityKg());
        if (updateReq.getCalSlope() != null) device.setCalSlope(updateReq.getCalSlope());
        if (updateReq.getCalZero() != null) device.setCalZero(updateReq.getCalZero());

        Device saved = deviceRepository.save(device);

        String clientIp = ClientIpResolver.resolveClientIp(request);
        if (persistenceService != null) {
            try {
                persistenceService.logAudit(
                        "DEVICE_UPDATED",
                        "Device updated: " + cleanEui,
                        "Name: " + saved.getName(),
                        clientIp
                );
            } catch (Exception e) {
                log.warn("Failed to audit device update: {}", e.getMessage());
            }
        }

        return ResponseEntity.ok(saved);
    }

    /**
     * DELETE /api/devices/{devEui} - Remove device (ROLE_ADMIN only)
     */
    @DeleteMapping("/{devEui}")
    public ResponseEntity<?> deleteDevice(
            @PathVariable String devEui,
            HttpServletRequest request
    ) {
        String cleanEui = devEui.toLowerCase().trim();
        if (!deviceRepository.existsById(cleanEui)) {
            return ResponseEntity.status(HttpStatus.NOT_FOUND).body(Map.of(
                    "error", "Not Found",
                    "message", "Dispositivo no encontrado con DevEUI: " + cleanEui
            ));
        }

        deviceRepository.deleteById(cleanEui);

        String clientIp = ClientIpResolver.resolveClientIp(request);
        if (persistenceService != null) {
            try {
                persistenceService.logAudit(
                        "DEVICE_DELETED",
                        "Device deleted: " + cleanEui,
                        "DevEUI: " + cleanEui,
                        clientIp
                );
            } catch (Exception e) {
                log.warn("Failed to audit device deletion: {}", e.getMessage());
            }
        }

        return ResponseEntity.ok(Map.of(
                "status", "SUCCESS",
                "message", "Dispositivo eliminado correctamente",
                "devEui", cleanEui
        ));
    }
}
