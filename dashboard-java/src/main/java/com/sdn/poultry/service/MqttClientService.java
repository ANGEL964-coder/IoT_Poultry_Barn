package com.sdn.poultry.service;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.sdn.poultry.model.PoultryTelemetry;
import com.sdn.poultry.model.SdnRuleRequest;
import com.sdn.poultry.protocol.SdnHeaderPacker;
import com.sdn.poultry.protocol.TelemetryDecoder;
import jakarta.annotation.PostConstruct;
import jakarta.annotation.PreDestroy;
import org.eclipse.paho.client.mqttv3.*;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.messaging.simp.SimpMessagingTemplate;
import org.springframework.stereotype.Service;

import java.nio.charset.StandardCharsets;
import java.time.Instant;
import java.time.format.DateTimeFormatter;
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.concurrent.atomic.AtomicLong;

@Service
public class MqttClientService implements MqttCallbackExtended {

    private static final Logger log = LoggerFactory.getLogger(MqttClientService.class);

    private final SimpMessagingTemplate messagingTemplate;
    private final ObjectMapper objectMapper = new ObjectMapper();

    @Autowired(required = false)
    private PersistenceService persistenceService;

    @Value("${app.mqtt.broker-url:tcp://127.0.0.1:1883}")
    private String brokerUrl = "tcp://127.0.0.1:1883";

    @Value("${app.mqtt.client-id:sdn-poultry-dashboard-backend}")
    private String clientId = "sdn-poultry-dashboard-backend";

    @Value("${app.mqtt.app-id:44d003ef-2bd1-480a-84cf-87ff6dc27506}")
    private String defaultAppId = "44d003ef-2bd1-480a-84cf-87ff6dc27506";

    @Value("${app.mqtt.uplink-topic:application/+/device/+/event/up}")
    private String uplinkTopic = "application/+/device/+/event/up";

    @Value("${app.poultry.hopper-capacity-kg:20.0}")
    private float hopperCapacityKg = 20.0f;

    private MqttAsyncClient mqttClient;
    private volatile boolean connected;
    private final AtomicLong receivedUplinks = new AtomicLong();

    // Cache del último estado por DevEUI
    private final Map<String, PoultryTelemetry> latestNodeStates = new ConcurrentHashMap<>();

    // Estado consolidado de la granja (acumula las últimas lecturas válidas de todos los nodos)
    private final PoultryTelemetry aggregatedFarmState = new PoultryTelemetry();

    // Historial reciente de telemetría por DevEUI (máximo 60 puntos por nodo)
    private final Map<String, List<PoultryTelemetry>> telemetryHistory = new ConcurrentHashMap<>();

    // Log de auditoría de comandos Downlink enviados
    private final List<Map<String, Object>> downlinkAuditLog = new CopyOnWriteArrayList<>();

    @Autowired
    public MqttClientService(SimpMessagingTemplate messagingTemplate,
                             @Autowired(required = false) PersistenceService persistenceService) {
        this.messagingTemplate = messagingTemplate;
        this.persistenceService = persistenceService;
    }

    public MqttClientService(SimpMessagingTemplate messagingTemplate) {
        this(messagingTemplate, null);
    }

    public void setPersistenceService(PersistenceService persistenceService) {
        this.persistenceService = persistenceService;
    }

    public PersistenceService getPersistenceService() {
        return persistenceService;
    }

    @PostConstruct
    public void init() {
        connectToBroker();
    }

    public synchronized void connectToBroker() {
        try {
            if (mqttClient != null && mqttClient.isConnected()) {
                return;
            }

            log.info("Conectando al Broker MQTT en: {}", brokerUrl);
            mqttClient = new MqttAsyncClient(brokerUrl, clientId + "-" + System.currentTimeMillis());
            mqttClient.setCallback(this);

            MqttConnectOptions options = new MqttConnectOptions();
            options.setAutomaticReconnect(true);
            options.setCleanSession(true);
            options.setConnectionTimeout(10);
            options.setKeepAliveInterval(30);

            IMqttToken token = mqttClient.connect(options);
            token.waitForCompletion(10000);
            connected = true;
            log.info("Conexión MQTT exitosa.");
        } catch (MqttException e) {
            log.error("Error al conectar con Broker MQTT (reintentará en background): {}", e.getMessage());
            connected = false;
        }
    }

    @Override
    public void connectComplete(boolean reconnect, String serverURI) {
        connected = true;
        log.info("Conexión establecida con Broker: {} (reconnect={})", serverURI, reconnect);
        try {
            mqttClient.subscribe(uplinkTopic, 1);
            log.info("Suscrito exitosamente al topico Uplink: {}", uplinkTopic);
            broadcastSystemStatus("CONNECTED", "Broker MQTT conectado y escuchando: " + uplinkTopic);
        } catch (MqttException e) {
            log.error("Error al suscribirse al topico {}: {}", uplinkTopic, e.getMessage());
        }
    }

    @Override
    public void connectionLost(Throwable cause) {
        connected = false;
        log.warn("Conexión con Broker MQTT perdida: {}", cause != null ? cause.getMessage() : "Desconocido");
        broadcastSystemStatus("DISCONNECTED", "Desconectado del Broker MQTT. Reintentando...");
    }

    @Override
    public void messageArrived(String topic, MqttMessage message) {
        try {
            String payloadStr = new String(message.getPayload(), StandardCharsets.UTF_8);
            JsonNode root = objectMapper.readTree(payloadStr);

            // 1. Extraer identificadores del dispositivo
            String devEui = root.path("deviceInfo").path("devEui").asText("");
            if (devEui.isEmpty()) {
                devEui = root.path("devEui").asText("");
            }
            if (devEui.isEmpty()) {
                // Extraer de topico: application/{app_id}/device/{dev_eui}/event/up
                String[] parts = topic.split("/");
                if (parts.length >= 4) {
                    devEui = parts[3];
                }
            }
            devEui = devEui.toLowerCase();

            String deviceName = root.path("deviceInfo").path("deviceName").asText(devEui);
            String fallbackAppId = defaultAppId != null ? defaultAppId : "44d003ef-2bd1-480a-84cf-87ff6dc27506";
            String applicationId = root.path("deviceInfo").path("applicationId").asText(fallbackAppId);
            if (applicationId == null || applicationId.isEmpty()) {
                String[] parts = topic.split("/");
                if (parts.length >= 2) {
                    applicationId = parts[1];
                }
            }
            long fCnt = root.path("fCnt").asLong(0);
            int fPort = root.path("fPort").asInt(0);

            // 2. Extraer métricas RF
            double rssi = -999.0;
            double snr = 0.0;
            String gatewayId = "N/A";
            JsonNode rxInfo = root.path("rxInfo");
            if (rxInfo.isArray() && rxInfo.size() > 0) {
                JsonNode bestRx = rxInfo.get(0);
                rssi = bestRx.path("rssi").asDouble(-999.0);
                snr = bestRx.path("snr").asDouble(0.0);
                gatewayId = bestRx.path("gatewayId").asText("N/A");
            }

            long freq = root.path("txInfo").path("frequency").asLong(0);
            int sf = root.path("txInfo").path("modulation").path("lora").path("spreadingFactor").asInt(7);

            // 3. Decodificar payload binario en Base64
            String dataB64 = root.path("data").asText("");
            if (dataB64.isEmpty()) {
                log.debug("Mensaje recibido sin payload de datos (MAC command o vacío). DevEUI: {}", devEui);
                return;
            }

            byte[] rawBytes = Base64.getDecoder().decode(dataB64);
            PoultryTelemetry telem = TelemetryDecoder.decode(rawBytes, hopperCapacityKg, devEui, fPort);

            // Completar con metadatos de red
            telem.setDevEui(devEui);
            telem.setDeviceName(deviceName);
            telem.setGatewayId(gatewayId);
            telem.setRssi(rssi);
            telem.setSnr(snr);
            telem.setFrequency(freq);
            telem.setSpreadingFactor(sf);
            telem.setFCnt(fCnt);
            telem.setFPort(fPort);

            // 4. Guardar en caché y lista de histórico
            receivedUplinks.incrementAndGet();
            latestNodeStates.put(devEui, telem);
            mergeIntoAggregatedState(telem);
            telemetryHistory.computeIfAbsent(devEui, k -> new CopyOnWriteArrayList<>()).add(telem);
            List<PoultryTelemetry> hist = telemetryHistory.get(devEui);
            while (hist.size() > 60) {
                hist.remove(0);
            }

            log.info("Uplink recibido de {} ({}) | {} bytes | SF: {} | RSSI: {} dBm",
                    devEui, deviceName, rawBytes.length, sf, rssi);

            // 5. Persistir telemetria y actualizar registro de dispositivo en PostgreSQL
            if (persistenceService != null) {
                try {
                    persistenceService.saveTelemetryAndUpsertDevice(telem, dataB64, applicationId);
                } catch (Exception pe) {
                    log.error("Error al persistir telemetria para DevEUI {}: {}", devEui, pe.getMessage(), pe);
                }
            }

            // 6. Emitir a los clientes WebSocket en tiempo real
            messagingTemplate.convertAndSend("/topic/telemetria", telem);
            messagingTemplate.convertAndSend("/topic/telemetria/" + devEui, telem);

        } catch (Exception e) {
            log.error("Error al procesar mensaje LoRaWAN MQTT: {}", e.getMessage(), e);
        }
    }

    @Override
    public void deliveryComplete(IMqttDeliveryToken token) {
        log.debug("Entrega de publicación MQTT confirmada.");
    }

    /**
     * Inyección de comando SDN Downlink hacia un nodo
     */
    public Map<String, Object> sendDownlink(SdnRuleRequest req) throws Exception {
        if (!connected || mqttClient == null || !mqttClient.isConnected()) {
            throw new IllegalStateException("Broker MQTT no conectado");
        }

        String targetDevEui = req.getDevEui().toLowerCase().trim();
        String appId = (req.getAppId() != null && !req.getAppId().isBlank()) ? req.getAppId().trim() : defaultAppId;

        // Empaquetar cabecera SDN de 32 bits (4 bytes Little-Endian)
        byte[] packedBytes = SdnHeaderPacker.pack(
                req.getCmdId(),
                req.getSfTarget(),
                req.getTxPower(),
                req.getSampleRate(),
                req.getPayloadMode(),
                req.getSleepEnable(),
                req.getParamValue()
        );

        String base64Payload = SdnHeaderPacker.toBase64(packedBytes);
        String hexDump = SdnHeaderPacker.toHex(packedBytes);

        // Construir JSON requerido por ChirpStack v4
        Map<String, Object> csPayload = new LinkedHashMap<>();
        csPayload.put("devEui", targetDevEui);
        csPayload.put("confirmed", req.isConfirmed());
        csPayload.put("fPort", req.getFPort());
        csPayload.put("data", base64Payload);

        String jsonPayload = objectMapper.writeValueAsString(csPayload);
        String downlinkTopic = String.format("application/%s/device/%s/command/down", appId, targetDevEui);

        MqttMessage message = new MqttMessage(jsonPayload.getBytes(StandardCharsets.UTF_8));
        message.setQos(1);
        mqttClient.publish(downlinkTopic, message).waitForCompletion(5000);

        log.info("Comando SDN Downlink publicado en {} -> Base64: {} | Hex: {}", downlinkTopic, base64Payload, hexDump);

        // Registro de auditoría
        Map<String, Object> audit = new LinkedHashMap<>();
        audit.put("timestamp", DateTimeFormatter.ISO_INSTANT.format(Instant.now()));
        audit.put("devEui", targetDevEui);
        audit.put("appId", appId);
        audit.put("cmdId", req.getCmdId());
        audit.put("cmdName", (req.getCmdId() < SdnHeaderPacker.CMD_NAMES.length) ? SdnHeaderPacker.CMD_NAMES[req.getCmdId()] : "CUSTOM");
        audit.put("sfTarget", req.getSfTarget());
        audit.put("sfName", (req.getSfTarget() < SdnHeaderPacker.SF_NAMES.length) ? SdnHeaderPacker.SF_NAMES[req.getSfTarget()] : "CUSTOM");
        audit.put("txPower", req.getTxPower());
        audit.put("sampleRate", req.getSampleRate());
        audit.put("payloadMode", req.getPayloadMode());
        audit.put("sleepEnable", req.getSleepEnable());
        audit.put("paramValue", req.getParamValue());
        audit.put("base64", base64Payload);
        audit.put("hex", hexDump);
        audit.put("status", "ENQUEUED_IN_CHIRPSTACK");

        downlinkAuditLog.add(0, audit);
        while (downlinkAuditLog.size() > 50) {
            downlinkAuditLog.remove(downlinkAuditLog.size() - 1);
        }

        // Persistir evento downlink en base de datos PostgreSQL
        if (persistenceService != null) {
            try {
                persistenceService.saveDownlinkEvent(req, hexDump, base64Payload, downlinkTopic, "ENQUEUED_IN_CHIRPSTACK");
            } catch (Exception pe) {
                log.error("Error al persistir downlink event para DevEUI {}: {}", targetDevEui, pe.getMessage(), pe);
            }
        }

        // Notificar por WebSocket que se envió una regla SDN
        messagingTemplate.convertAndSend("/topic/sdn/downlink-events", audit);

        return audit;
    }

    private void broadcastSystemStatus(String status, String message) {
        Map<String, Object> msg = new HashMap<>();
        msg.put("status", status);
        msg.put("message", message);
        msg.put("timestamp", Instant.now().toString());
        messagingTemplate.convertAndSend("/topic/status", msg);
    }

    public Map<String, PoultryTelemetry> getLatestNodeStates() {
        return Collections.unmodifiableMap(latestNodeStates);
    }

    public List<PoultryTelemetry> getNodeHistory(String devEui) {
        List<PoultryTelemetry> list = new ArrayList<>(telemetryHistory.getOrDefault(devEui.toLowerCase(), Collections.emptyList()));
        return list;
    }

    public List<Map<String, Object>> getDownlinkAuditLog() {
        return Collections.unmodifiableList(downlinkAuditLog);
    }

    public long getReceivedUplinks() {
        return receivedUplinks.get();
    }

    public boolean isConnected() {
        return connected;
    }

    public synchronized PoultryTelemetry getAggregatedFarmState() {
        return aggregatedFarmState;
    }

    private synchronized void mergeIntoAggregatedState(PoultryTelemetry telem) {
        if (telem == null) return;
        aggregatedFarmState.setTimestamp(telem.getTimestamp());
        if (telem.getGatewayId() != null) aggregatedFarmState.setGatewayId(telem.getGatewayId());
        if (telem.getBatteryMv() > 0) aggregatedFarmState.setBatteryMv(telem.getBatteryMv());

        // Conteo de huevos (Visión)
        if (telem.getTotalEggs() != null) {
            aggregatedFarmState.setTotalEggs(telem.getTotalEggs());
            if (telem.getCamHour() != null) aggregatedFarmState.setCamHour(telem.getCamHour());
            if (telem.getCamMinute() != null) aggregatedFarmState.setCamMinute(telem.getCamMinute());
            if (telem.getEggsPerMinute() != null) aggregatedFarmState.setEggsPerMinute(telem.getEggsPerMinute());
            if (telem.getUartStatus() != null) aggregatedFarmState.setUartStatus(telem.getUartStatus());
            if (telem.getTotalDetections() != null) aggregatedFarmState.setTotalDetections(telem.getTotalDetections());
        }

        // Datos de Balanza (Peso / Tolva)
        if (telem.getWeightKg() != null && telem.getWeightKg() > 0.0f) {
            aggregatedFarmState.setWeightKg(telem.getWeightKg());
            if (telem.getGrossWeightKg() != null) aggregatedFarmState.setGrossWeightKg(telem.getGrossWeightKg());
            if (telem.getVoltageV() != null) aggregatedFarmState.setVoltageV(telem.getVoltageV());
            if (telem.getAdcRaw() != null) aggregatedFarmState.setAdcRaw(telem.getAdcRaw());
            if (telem.getCalSlope() != null) aggregatedFarmState.setCalSlope(telem.getCalSlope());
            if (telem.getCalZero() != null) aggregatedFarmState.setCalZero(telem.getCalZero());
            if (telem.getTimeOnAirMs() != null) aggregatedFarmState.setTimeOnAirMs(telem.getTimeOnAirMs());
            if (telem.getHopperPercentage() != null) aggregatedFarmState.setHopperPercentage(telem.getHopperPercentage());
        }

        // Datos Ambientales (Temperatura / Humedad)
        // Ignorar nodos de huevos y no permitir que datos simulados de balanza sobrescriban mediciones reales
        boolean isEgg = TelemetryDecoder.isEggCounterDevice(telem.getDevEui(), telem.getFPort());
        boolean isScale = (telem.getDevEui() != null && telem.getDevEui().contains("70b3d57ed007985a"));
        if (!isEgg && telem.getTemperature() != null && telem.getTemperature() > 0.0f) {
            if (!isScale || aggregatedFarmState.getTemperature() == null) {
                aggregatedFarmState.setTemperature(telem.getTemperature());
                if (telem.getHumidity() != null) aggregatedFarmState.setHumidity(telem.getHumidity());
                if (telem.getPressure() != null) aggregatedFarmState.setPressure(telem.getPressure());
            }
        }
    }

    @PreDestroy
    public void cleanup() {
        try {
            if (mqttClient != null && mqttClient.isConnected()) {
                mqttClient.disconnect();
                mqttClient.close();
            }
        } catch (MqttException ignored) {}
    }
}
