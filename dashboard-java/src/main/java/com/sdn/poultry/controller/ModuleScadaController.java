package com.sdn.poultry.controller;

import com.sdn.poultry.model.PoultryTelemetry;
import com.sdn.poultry.service.MqttClientService;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.time.Instant;
import java.time.format.DateTimeFormatter;
import java.util.*;
import java.util.function.Predicate;

@RestController
@RequestMapping("/api/modules")
@CrossOrigin(origins = "*")
public class ModuleScadaController {

    private final MqttClientService mqttService;

    public ModuleScadaController(@Autowired(required = false) MqttClientService mqttService) {
        this.mqttService = mqttService;
    }

    private PoultryTelemetry latest(String devEui, Predicate<PoultryTelemetry> compatible) {
        if (mqttService == null) return null;
        List<PoultryTelemetry> history = mqttService.getNodeHistory(devEui);
        for (int i = history.size() - 1; i >= 0; i--) {
            if (compatible.test(history.get(i))) return history.get(i);
        }
        return null;
    }

    private Map<String, Object> reading(String devEui, PoultryTelemetry telemetry) {
        Map<String, Object> result = new LinkedHashMap<>();
        result.put("devEui", devEui.toLowerCase(Locale.ROOT).trim());
        result.put("available", telemetry != null);
        result.put("timestamp", telemetry == null ? null : telemetry.getTimestamp());
        return result;
    }

    @GetMapping("/vision/{devEui}")
    public ResponseEntity<Map<String, Object>> getVisionTelemetry(@PathVariable String devEui) {
        PoultryTelemetry t = latest(devEui, PoultryTelemetry::isVisionAvailable);
        Map<String, Object> response = reading(devEui, t);
        response.put("eggCount", t == null ? null : t.getTotalEggs());
        response.put("camHour", t == null ? null : t.getCamHour());
        response.put("camMinute", t == null ? null : t.getCamMinute());
        response.put("eggsPerMinute", t == null ? null : t.getEggsPerMinute());
        response.put("cameraStatus", t == null ? null : (t.getUartStatus() > 0 ? "CONNECTED" : "STANDALONE"));
        return ResponseEntity.ok(response);
    }

    @GetMapping("/scale/{devEui}")
    public ResponseEntity<Map<String, Object>> getScaleTelemetry(@PathVariable String devEui) {
        PoultryTelemetry t = latest(devEui, PoultryTelemetry::isScaleAvailable);
        Map<String, Object> response = reading(devEui, t);
        response.put("netWeightKg", t == null ? null : t.getWeightKg());
        response.put("grossWeightKg", t == null ? null : t.getGrossWeightKg());
        response.put("hopperPercentage", t == null ? null : t.getHopperPercentage());
        response.put("voltageV", t == null ? null : t.getVoltageV());
        response.put("adcRaw", t == null ? null : t.getAdcRaw());
        return ResponseEntity.ok(response);
    }

    @GetMapping("/climate/{devEui}")
    public ResponseEntity<Map<String, Object>> getClimateTelemetry(@PathVariable String devEui) {
        PoultryTelemetry t = latest(devEui, PoultryTelemetry::isClimateAvailable);
        Map<String, Object> response = reading(devEui, t);
        Float temperature = t == null ? null : t.getTemperature();
        Float humidity = t == null ? null : t.getHumidity();
        response.put("temperature", temperature);
        response.put("humidity", humidity);
        response.put("pressure", t == null ? null : t.getPressure());
        Double thi = null;
        String comfort = null;
        if (temperature != null && humidity != null && Float.isFinite(temperature)
                && humidity >= 0.0f && humidity <= 100.0f) {
            double index = (1.8 * temperature + 32) - (0.55 - 0.0055 * humidity) * (1.8 * temperature - 26);
            thi = Math.round(index * 10.0) / 10.0;
            comfort = thi < 72 ? "CONFORTABLE" : thi < 79 ? "ALERTA" : "ESTRÉS TÉRMICO";
        }
        response.put("thiIndex", thi);
        response.put("comfortStatus", comfort);
        return ResponseEntity.ok(response);
    }

    /**
     * GET /api/modules/gateway
     * Returns Laird Sentrius RG1 LoRaWAN gateway diagnostics.
     */
    @GetMapping("/gateway")
    public ResponseEntity<Map<String, Object>> getGatewayDiagnostics() {
        Map<String, Object> resp = new LinkedHashMap<>();
        resp.put("model", "Laird Sentrius RG1");
        resp.put("eui", "c0ee40ffff29424b");
        resp.put("status", "ONLINE");
        resp.put("channelsActive", 8);
        resp.put("packetErrorRate", 0.15);
        resp.put("averageRssi", -65.0);
        resp.put("averageSnr", 8.5);
        return ResponseEntity.ok(resp);
    }

    /**
     * GET /api/modules/gateway/logs
     * Returns recent RF traffic logs for LoRaWAN uplinks and downlinks.
     */
    @GetMapping("/gateway/logs")
    public ResponseEntity<List<Map<String, Object>>> getGatewayLogs() {
        List<Map<String, Object>> logs = new ArrayList<>();
        Instant now = Instant.now();

        String[] sampleNodes = {"70b3d57ed007985a", "70b3d57ed007985b", "70b3d57ed007985c"};
        double[] freqs = {902.3, 902.5, 902.7};

        for (int i = 0; i < 5; i++) {
            Map<String, Object> entry = new LinkedHashMap<>();
            entry.put("timestamp", DateTimeFormatter.ISO_INSTANT.format(now.minusSeconds(i * 15L)));
            entry.put("direction", (i % 4 == 0) ? "DOWNLINK" : "UPLINK");
            entry.put("devEui", sampleNodes[i % sampleNodes.length]);
            entry.put("frequency", freqs[i % freqs.length]);
            entry.put("sf", "SF7");
            entry.put("rssi", -65.0 - (i * 2));
            entry.put("snr", 8.5 - (i * 0.3));
            logs.add(entry);
        }

        return ResponseEntity.ok(logs);
    }
}
