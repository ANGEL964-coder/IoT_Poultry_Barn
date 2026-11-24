package com.sdn.poultry.repository;

import com.sdn.poultry.entity.SdnRuleEvent;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.orm.jpa.DataJpaTest;
import org.springframework.test.context.TestPropertySource;

import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.List;

import static org.assertj.core.api.Assertions.assertThat;

@DataJpaTest
@TestPropertySource(locations = "classpath:application-test.properties")
public class SdnRuleEventRepositoryTest {

    @Autowired
    private SdnRuleEventRepository sdnRuleEventRepository;

    @Test
    @DisplayName("Should persist and retrieve 32-bit Little-Endian SDN rule events")
    public void testSaveAndFindSdnRuleEvent() {
        SdnRuleEvent event = new SdnRuleEvent();
        event.setDevEui("70b3d57ed007985a");
        event.setAppId("44d003ef-2bd1-480a-84cf-87ff6dc27506");
        event.setCmdId(1);
        event.setCmdName("RECONFIG_RADIO");
        event.setSfTarget(2); // SF9
        event.setSfName("SF9");
        event.setTxPower(14);
        event.setSampleRate(1); // 60s
        event.setPayloadMode(1); // CRITICAL
        event.setSleepEnable(false);
        event.setParamValue(0);
        event.setRawWordHex("0x00000711");
        event.setBase64Payload("EQcAAA==");
        event.setTopic("application/44d003ef-2bd1-480a-84cf-87ff6dc27506/device/70b3d57ed007985a/command/down");
        event.setStatus("ENQUEUED_IN_CHIRPSTACK");
        event.setSentAt(Instant.now());

        SdnRuleEvent saved = sdnRuleEventRepository.save(event);
        assertThat(saved.getId()).isNotNull();

        List<SdnRuleEvent> events = sdnRuleEventRepository.findByDevEuiOrderBySentAtDesc("70b3d57ed007985a");
        assertThat(events).hasSize(1);
        SdnRuleEvent retrieved = events.get(0);
        assertThat(retrieved.getCmdId()).isEqualTo(1);
        assertThat(retrieved.getCmdName()).isEqualTo("RECONFIG_RADIO");
        assertThat(retrieved.getSfTarget()).isEqualTo(2);
        assertThat(retrieved.getSfName()).isEqualTo("SF9");
        assertThat(retrieved.getTxPower()).isEqualTo(14);
        assertThat(retrieved.getRawWordHex()).isEqualTo("0x00000711");
        assertThat(retrieved.getBase64Payload()).isEqualTo("EQcAAA==");
        assertThat(retrieved.getStatus()).isEqualTo("ENQUEUED_IN_CHIRPSTACK");
    }

    @Test
    @DisplayName("Should order downlink events chronologically descending")
    public void testOrderingBySentAtDesc() {
        String devEui = "70b3d57ed0079802";
        Instant now = Instant.now();

        SdnRuleEvent ev1 = new SdnRuleEvent();
        ev1.setDevEui(devEui);
        ev1.setCmdId(1);
        ev1.setSentAt(now.minus(10, ChronoUnit.MINUTES));
        sdnRuleEventRepository.save(ev1);

        SdnRuleEvent ev2 = new SdnRuleEvent();
        ev2.setDevEui(devEui);
        ev2.setCmdId(2);
        ev2.setSentAt(now.minus(5, ChronoUnit.MINUTES));
        sdnRuleEventRepository.save(ev2);

        SdnRuleEvent ev3 = new SdnRuleEvent();
        ev3.setDevEui(devEui);
        ev3.setCmdId(3);
        ev3.setSentAt(now);
        sdnRuleEventRepository.save(ev3);

        List<SdnRuleEvent> list = sdnRuleEventRepository.findByDevEuiOrderBySentAtDesc(devEui);
        assertThat(list).hasSize(3);
        assertThat(list.get(0).getCmdId()).isEqualTo(3);
        assertThat(list.get(1).getCmdId()).isEqualTo(2);
        assertThat(list.get(2).getCmdId()).isEqualTo(1);
    }
}
