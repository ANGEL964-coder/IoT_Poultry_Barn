package com.sdn.poultry.repository;

import com.sdn.poultry.entity.Device;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.orm.jpa.DataJpaTest;
import org.springframework.test.context.TestPropertySource;

import java.time.Instant;
import java.util.List;
import java.util.Optional;

import static org.assertj.core.api.Assertions.assertThat;

@DataJpaTest
@TestPropertySource(locations = "classpath:application-test.properties")
public class DeviceRepositoryTest {

    @Autowired
    private DeviceRepository deviceRepository;

    @Test
    @DisplayName("Should persist and retrieve a device by devEui")
    public void testSaveAndFindDevice() {
        Device device = new Device("70b3d57ed007985a", "Galpón 1 Central", "44d003ef-2bd1-480a-84cf-87ff6dc27506", "ONLINE");
        device.setHopperCapacityKg(25.0f);
        device.setCalSlope(0.045f);
        device.setCalZero(0.512f);

        Device saved = deviceRepository.save(device);
        assertThat(saved).isNotNull();

        Optional<Device> found = deviceRepository.findById("70b3d57ed007985a");
        assertThat(found).isPresent();
        assertThat(found.get().getName()).isEqualTo("Galpón 1 Central");
        assertThat(found.get().getApplicationId()).isEqualTo("44d003ef-2bd1-480a-84cf-87ff6dc27506");
        assertThat(found.get().getStatus()).isEqualTo("ONLINE");
        assertThat(found.get().getHopperCapacityKg()).isEqualTo(25.0f);
        assertThat(found.get().getCalSlope()).isEqualTo(0.045f);
        assertThat(found.get().getCalZero()).isEqualTo(0.512f);
        assertThat(found.get().getCreatedAt()).isNotNull();
        assertThat(found.get().getLastSeen()).isNotNull();
    }

    @Test
    @DisplayName("Should normalize devEui to lowercase on persist")
    public void testNormalizeDevEui() {
        Device device = new Device("70B3D57ED007985A ", "Galpón 2", "app-1", "ONLINE");
        deviceRepository.saveAndFlush(device);

        Optional<Device> found = deviceRepository.findById("70b3d57ed007985a");
        assertThat(found).isPresent();
        assertThat(found.get().getDevEui()).isEqualTo("70b3d57ed007985a");
    }

    @Test
    @DisplayName("Should filter devices by status and application ID")
    public void testFindByStatusAndApplicationId() {
        Device d1 = new Device("70b3d57ed0079801", "Galpón 1", "app-sdn-prod", "ONLINE");
        Device d2 = new Device("70b3d57ed0079802", "Galpón 2", "app-sdn-prod", "OFFLINE");
        Device d3 = new Device("70b3d57ed0079803", "Galpón 3", "app-sdn-dev", "ONLINE");

        deviceRepository.save(d1);
        deviceRepository.save(d2);
        deviceRepository.save(d3);

        List<Device> onlineDevices = deviceRepository.findByStatus("ONLINE");
        assertThat(onlineDevices).hasSize(2);

        List<Device> offlineDevices = deviceRepository.findByStatus("OFFLINE");
        assertThat(offlineDevices).hasSize(1);
        assertThat(offlineDevices.get(0).getDevEui()).isEqualTo("70b3d57ed0079802");

        List<Device> prodDevices = deviceRepository.findByApplicationId("app-sdn-prod");
        assertThat(prodDevices).hasSize(2);
    }
}
