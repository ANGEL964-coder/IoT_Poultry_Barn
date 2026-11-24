package com.sdn.poultry.repository;

import com.sdn.poultry.entity.AuditLog;
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
public class AuditLogRepositoryTest {

    @Autowired
    private AuditLogRepository auditLogRepository;

    @Test
    @DisplayName("Should persist and filter operational audit logs")
    public void testSaveAndFindAuditLog() {
        AuditLog log = new AuditLog(
                "SDN_DOWNLINK",
                "Downlink injected to 70b3d57ed007985a",
                "{\"cmd\":1,\"sf\":7,\"tx\":14}",
                "192.168.1.100"
        );

        AuditLog saved = auditLogRepository.save(log);
        assertThat(saved.getId()).isNotNull();

        List<AuditLog> list = auditLogRepository.findByActionOrderByTimestampDesc("SDN_DOWNLINK");
        assertThat(list).hasSize(1);
        assertThat(list.get(0).getDescription()).isEqualTo("Downlink injected to 70b3d57ed007985a");
        assertThat(list.get(0).getIp()).isEqualTo("192.168.1.100");
        assertThat(list.get(0).getTimestamp()).isNotNull();
    }

    @Test
    @DisplayName("Should retrieve top recent logs in descending order")
    public void testTopLogsDescending() {
        Instant now = Instant.now();
        for (int i = 1; i <= 5; i++) {
            AuditLog l = new AuditLog("ACTION_" + i, "Desc " + i, null, "127.0.0.1");
            l.setTimestamp(now.minus(5 - i, ChronoUnit.MINUTES));
            auditLogRepository.save(l);
        }

        List<AuditLog> recent = auditLogRepository.findTop50ByOrderByTimestampDesc();
        assertThat(recent).hasSize(5);
        assertThat(recent.get(0).getAction()).isEqualTo("ACTION_5");
        assertThat(recent.get(4).getAction()).isEqualTo("ACTION_1");
    }
}
