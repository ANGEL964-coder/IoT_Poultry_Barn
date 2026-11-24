package com.sdn.poultry.controller;

import com.sdn.poultry.model.PoultryTelemetry;
import com.sdn.poultry.model.SdnRuleRequest;
import com.sdn.poultry.protocol.SdnHeaderPacker;
import com.sdn.poultry.service.MqttClientService;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.*;

@RestController
@RequestMapping("/api")
@CrossOrigin(origins = "*")
public class DashboardRestController {

    private final MqttClientService mqttService;

    public DashboardRestController(MqttClientService mqttService) {
        this.mqttService = mqttService;
    }

    @GetMapping("/status")
    public ResponseEntity<Map<String, Object>> getStatus() {
        Map<String, Object> resp = new HashMap<>();
        resp.put("status", "healthy");
        resp.put("mqttConnected", mqttService.isConnected());
        resp.put("activeNodesCount", mqttService.getLatestNodeStates().size());
        resp.put("serverTime", new Date());
        return ResponseEntity.ok(resp);
    }

    @GetMapping("/stats")
    public ResponseEntity<Map<String, Object>> getStats() {
        Map<String, Object> resp = new HashMap<>();
        resp.put("totalUplinks", mqttService.getReceivedUplinks());
        resp.put("totalDownlinks", mqttService.getDownlinkAuditLog().size());
        resp.put("activeDevices", mqttService.getLatestNodeStates().size());
        return ResponseEntity.ok(resp);
    }

    @GetMapping("/galpones")
    public ResponseEntity<Collection<PoultryTelemetry>> getAllGalpones() {
        return ResponseEntity.ok(mqttService.getLatestNodeStates().values());
    }

    @GetMapping("/galpones/aggregated")
    public ResponseEntity<PoultryTelemetry> getAggregatedGalpon() {
        return ResponseEntity.ok(mqttService.getAggregatedFarmState());
    }

    @GetMapping("/galpones/{devEui}")
    public ResponseEntity<PoultryTelemetry> getGalponByEui(@PathVariable String devEui) {
        PoultryTelemetry telem = mqttService.getLatestNodeStates().get(devEui.toLowerCase());
        if (telem == null) {
            return ResponseEntity.notFound().build();
        }
        return ResponseEntity.ok(telem);
    }

    @GetMapping("/galpones/{devEui}/history")
    public ResponseEntity<List<PoultryTelemetry>> getGalponHistory(@PathVariable String devEui) {
        return ResponseEntity.ok(mqttService.getNodeHistory(devEui));
    }

    @GetMapping("/sdn/downlinks")
    public ResponseEntity<List<Map<String, Object>>> getDownlinkHistory() {
        return ResponseEntity.ok(mqttService.getDownlinkAuditLog());
    }

    @PostMapping("/sdn/preview")
    public ResponseEntity<Map<String, Object>> previewSdnHeader(@RequestBody SdnRuleRequest req) {
        byte[] packed = SdnHeaderPacker.pack(
                req.getCmdId(),
                req.getSfTarget(),
                req.getTxPower(),
                req.getSampleRate(),
                req.getPayloadMode(),
                req.getSleepEnable(),
                req.getParamValue()
        );
        Map<String, Object> resp = SdnHeaderPacker.unpack(packed);
        resp.put("hexDump", SdnHeaderPacker.toHex(packed));
        resp.put("base64", SdnHeaderPacker.toBase64(packed));
        return ResponseEntity.ok(resp);
    }

    @PostMapping("/sdn/downlink")
    public ResponseEntity<?> sendSdnDownlink(@RequestBody SdnRuleRequest req) {
        if (req.getDevEui() == null || req.getDevEui().isBlank()) {
            return ResponseEntity.badRequest().body(Map.of("error", "DevEUI es obligatorio"));
        }
        try {
            Map<String, Object> result = mqttService.sendDownlink(req);
            return ResponseEntity.ok(result);
        } catch (Exception e) {
            return ResponseEntity.internalServerError().body(Map.of("error", e.getMessage()));
        }
    }
}
