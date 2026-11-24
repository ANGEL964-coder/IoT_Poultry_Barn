package com.sdn.poultry.repository;

import com.sdn.poultry.entity.Device;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.stereotype.Repository;

import java.util.List;
import java.util.Optional;

@Repository
public interface DeviceRepository extends JpaRepository<Device, String> {
    Optional<Device> findByDevEui(String devEui);
    List<Device> findByStatus(String status);
    List<Device> findByApplicationId(String applicationId);
}
