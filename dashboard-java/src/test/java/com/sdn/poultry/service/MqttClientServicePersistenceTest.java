package com.sdn.poultry.service;

import com.sdn.poultry.model.PoultryTelemetry;
import com.sdn.poultry.model.SdnRuleRequest;
import org.eclipse.paho.client.mqttv3.MqttMessage;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.extension.ExtendWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.jupiter.MockitoExtension;
import org.springframework.messaging.simp.SimpMessagingTemplate;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.Base64;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatCode;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.*;

@ExtendWith(MockitoExtension.class)
public class MqttClientServicePersistenceTest {

    @Mock
    private SimpMessagingTemplate messagingTemplate;

    @Mock
    private PersistenceService persistenceService;

    private MqttClientService mqttClientService;

    @BeforeEach
    public void setUp() {
        mqttClientService = new MqttClientService(messagingTemplate, persistenceService);
    }

    @Test
    @DisplayName("Should invoke persistenceService.saveTelemetryAndUpsertDevice on valid MQTT uplink frame")
    public void testMessageArrivedPersistsTelemetry() {
        // Construct 48-byte Little-Endian binary payload
        ByteBuffer buf = ByteBuffer.allocate(48).order(ByteOrder.LITTLE_ENDIAN);
        buf.putInt(42);           // seq
        buf.putInt(180000);       // uptime_ms
        buf.putFloat(3280.0f);    // battery_mv
        buf.putFloat(27.3f);      // temperature
        buf.putFloat(65.0f);      // humidity
        buf.putFloat(1011.5f);    // pressure
        buf.putFloat(16.5f);      // weight_kg
        buf.putFloat(18.0f);      // gross_weight_kg
        buf.putFloat(1.35f);      // voltage_v
        buf.putShort((short) 1620); // adc_raw
        buf.putFloat(0.0500f);    // cal_slope
        buf.putFloat(0.5000f);    // cal_zero
        buf.putShort((short) 62); // time_on_air_ms

        String base64Data = Base64.getEncoder().encodeToString(buf.array());

        String jsonEnvelope = "{\n" +
                "  \"deviceInfo\": {\n" +
                "    \"applicationId\": \"44d003ef-2bd1-480a-84cf-87ff6dc27506\",\n" +
                "    \"devEui\": \"70B3D57ED007985A\",\n" +
                "    \"deviceName\": \"heltec-v3-poultry-01\"\n" +
                "  },\n" +
                "  \"fCnt\": 105,\n" +
                "  \"fPort\": 10,\n" +
                "  \"rxInfo\": [{\"rssi\": -80.0, \"snr\": 9.0, \"gatewayId\": \"gw-alpha-01\"}],\n" +
                "  \"txInfo\": {\"frequency\": 902300000, \"modulation\": {\"lora\": {\"spreadingFactor\": 7}}},\n" +
                "  \"data\": \"" + base64Data + "\"\n" +
                "}";

        MqttMessage message = new MqttMessage(jsonEnvelope.getBytes(StandardCharsets.UTF_8));
        String topic = "application/44d003ef-2bd1-480a-84cf-87ff6dc27506/device/70b3d57ed007985a/event/up";

        mqttClientService.messageArrived(topic, message);

        ArgumentCaptor<PoultryTelemetry> telemCaptor = ArgumentCaptor.forClass(PoultryTelemetry.class);
        verify(persistenceService, times(1)).saveTelemetryAndUpsertDevice(
                telemCaptor.capture(),
                eq(base64Data),
                eq("44d003ef-2bd1-480a-84cf-87ff6dc27506")
        );

        PoultryTelemetry captured = telemCaptor.getValue();
        assertThat(captured.getDevEui()).isEqualTo("70b3d57ed007985a");
        assertThat(captured.getSeq()).isEqualTo(42L);
        assertThat(captured.getTemperature()).isEqualTo(27.3f);
        assertThat(captured.getWeightKg()).isEqualTo(16.5f);
        assertThat(captured.getRssi()).isEqualTo(-80.0);

        // Verify WebSocket broadcast still occurs
        verify(messagingTemplate, atLeastOnce()).convertAndSend(eq("/topic/telemetria"), any(PoultryTelemetry.class));
    }

    @Test
    @DisplayName("Should be resilient against database exceptions during messageArrived")
    public void testResilienceOnDatabaseFailure() {
        ByteBuffer buf = ByteBuffer.allocate(24).order(ByteOrder.LITTLE_ENDIAN);
        buf.putInt(1);
        buf.putInt(1000);
        buf.putFloat(3300.0f);
        buf.putFloat(20.0f);
        buf.putFloat(50.0f);
        buf.putFloat(1013.0f);

        String base64Data = Base64.getEncoder().encodeToString(buf.array());
        String json = "{\"deviceInfo\":{\"devEui\":\"70b3d57ed007985a\"},\"data\":\"" + base64Data + "\"}";
        MqttMessage message = new MqttMessage(json.getBytes(StandardCharsets.UTF_8));

        // Configure mock to throw an exception
        doThrow(new RuntimeException("Database connection timeout"))
                .when(persistenceService).saveTelemetryAndUpsertDevice(any(), any(), any());

        // messageArrived must catch the exception and not propagate it to the MQTT thread
        assertThatCode(() -> mqttClientService.messageArrived("test/topic", message))
                .doesNotThrowAnyException();
    }
}
