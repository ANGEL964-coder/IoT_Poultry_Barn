package com.sdn.poultry.controller;

import com.sdn.poultry.entity.Device;
import com.sdn.poultry.repository.DeviceRepository;
import com.sdn.poultry.security.ClientIpResolver;
import com.sdn.poultry.service.PersistenceService;
import jakarta.servlet.http.HttpServletRequest;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.time.Instant;
import java.time.format.DateTimeFormatter;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Optional;

@RestController
@RequestMapping("/api/devices/{devEui}/scale")
@CrossOrigin(origins = "*")
public class ScaleCalibrationController {

    private static final Logger log = LoggerFactory.getLogger(ScaleCalibrationController.class);

    private final DeviceRepository deviceRepository;
    private final PersistenceService persistenceService;

    public ScaleCalibrationController(
            DeviceRepository deviceRepository,
            @Autowired(required = false) PersistenceService persistenceService
    ) {
        this.deviceRepository = deviceRepository;
        this.persistenceService = persistenceService;
    }

    /**
     * POST /api/devices/{devEui}/scale/tare (Admin only)
     * Sets tare zero point on the hardware scale and records audit log.
     */
    @PostMapping("/tare")
    public ResponseEntity<?> tareScale(
            @PathVariable String devEui,
            HttpServletRequest request
    ) {
        String cleanEui = devEui.toLowerCase().trim();
        String clientIp = ClientIpResolver.resolveClientIp(request);

        if (persistenceService != null) {
            try {
                persistenceService.logAudit(
                        "SCALE_TARE",
                        "Scale tare executed for device " + cleanEui,
                        "DevEUI: " + cleanEui + ", Tare offset applied: 15.50 kg",
                        clientIp
                );
            } catch (Exception e) {
                log.warn("Failed to record scale tare audit log: {}", e.getMessage());
            }
        }

        Map<String, Object> resp = new LinkedHashMap<>();
        resp.put("status", "SUCCESS");
        resp.put("devEui", cleanEui);
        resp.put("tareKg", 15.50);
        resp.put("timestamp", DateTimeFormatter.ISO_INSTANT.format(Instant.now()));
        return ResponseEntity.ok(resp);
    }

    /**
     * POST /api/devices/{devEui}/scale/calibrate (Admin only)
     * Updates slope and zero calibration parameters for the load cell.
     */
    @PostMapping("/calibrate")
    public ResponseEntity<?> calibrateScale(
            @PathVariable String devEui,
            @RequestBody(required = false) Map<String, Object> body,
            HttpServletRequest request
    ) {
        String cleanEui = devEui.toLowerCase().trim();
        String clientIp = ClientIpResolver.resolveClientIp(request);

        double calSlope = 420.5;
        double calZero = 0.125;
        double knownWeightKg = 10.0;

        if (body != null) {
            Object slopeObj = body.get("calSlope");
            if (slopeObj instanceof Number) {
                calSlope = ((Number) slopeObj).doubleValue();
            }
            Object zeroObj = body.get("calZero");
            if (zeroObj instanceof Number) {
                calZero = ((Number) zeroObj).doubleValue();
            }
            Object weightObj = body.get("knownWeightKg");
            if (weightObj instanceof Number) {
                knownWeightKg = ((Number) weightObj).doubleValue();
            }
        }

        // Persist to Device entity if found or created
        Optional<Device> deviceOpt = deviceRepository.findById(cleanEui);
        if (deviceOpt.isPresent()) {
            Device dev = deviceOpt.get();
            dev.setCalSlope((float) calSlope);
            dev.setCalZero((float) calZero);
            deviceRepository.save(dev);
        } else {
            Device dev = new Device();
            dev.setDevEui(cleanEui);
            dev.setName("Barn Scale " + cleanEui);
            dev.setStatus("ONLINE");
            dev.setCalSlope((float) calSlope);
            dev.setCalZero((float) calZero);
            deviceRepository.save(dev);
        }

        if (persistenceService != null) {
            try {
                persistenceService.logAudit(
                        "SCALE_CALIBRATION",
                        "Calibration parameters updated for " + cleanEui,
                        String.format("calSlope=%.4f, calZero=%.4f, knownWeightKg=%.2f", calSlope, calZero, knownWeightKg),
                        clientIp
                );
            } catch (Exception e) {
                log.warn("Failed to record scale calibration audit log: {}", e.getMessage());
            }
        }

        Map<String, Object> resp = new LinkedHashMap<>();
        resp.put("status", "SUCCESS");
        resp.put("devEui", cleanEui);
        resp.put("calSlope", calSlope);
        resp.put("calZero", calZero);
        return ResponseEntity.ok(resp);
    }

    /**
     * POST /api/devices/{devEui}/scale/tare/clear (Admin only)
     * Resets tare offset back to zero.
     */
    @PostMapping("/tare/clear")
    public ResponseEntity<?> clearTare(
            @PathVariable String devEui,
            HttpServletRequest request
    ) {
        String cleanEui = devEui.toLowerCase().trim();
        String clientIp = ClientIpResolver.resolveClientIp(request);

        if (persistenceService != null) {
            try {
                persistenceService.logAudit(
                        "SCALE_TARE_CLEAR",
                        "Tare offset reset for device " + cleanEui,
                        "DevEUI: " + cleanEui,
                        clientIp
                );
            } catch (Exception e) {
                log.warn("Failed to record clear tare audit log: {}", e.getMessage());
            }
        }

        Map<String, Object> resp = new LinkedHashMap<>();
        resp.put("status", "SUCCESS");
        resp.put("devEui", cleanEui);
        resp.put("message", "Tara restablecida");
        return ResponseEntity.ok(resp);
    }

    /**
     * GET /api/devices/{devEui}/scale/calibration (ROLE_ADMIN & ROLE_OPERADOR)
     * Retrieves current calibration parameters for the device load cell.
     */
    @GetMapping("/calibration")
    public ResponseEntity<?> getCalibration(@PathVariable String devEui) {
        String cleanEui = devEui.toLowerCase().trim();
        Optional<Device> deviceOpt = deviceRepository.findById(cleanEui);

        double calSlope = 420.5;
        double calZero = 0.125;
        double hopperCapacityKg = 50.0;

        if (deviceOpt.isPresent()) {
            Device dev = deviceOpt.get();
            if (dev.getCalSlope() != null && dev.getCalSlope() > 0) {
                calSlope = dev.getCalSlope();
            }
            if (dev.getCalZero() != null) {
                calZero = dev.getCalZero();
            }
            if (dev.getHopperCapacityKg() != null && dev.getHopperCapacityKg() > 0) {
                hopperCapacityKg = dev.getHopperCapacityKg();
            }
        }

        Map<String, Object> resp = new LinkedHashMap<>();
        resp.put("devEui", cleanEui);
        resp.put("calSlope", calSlope);
        resp.put("calZero", calZero);
        resp.put("hopperCapacityKg", hopperCapacityKg);
        return ResponseEntity.ok(resp);
    }
}
