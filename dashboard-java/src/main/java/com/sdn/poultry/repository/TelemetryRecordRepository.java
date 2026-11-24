package com.sdn.poultry.repository;

import com.sdn.poultry.entity.TelemetryRecord;
import org.springframework.data.domain.Pageable;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.stereotype.Repository;

import java.time.Instant;
import java.util.List;
import java.util.Optional;

@Repository
public interface TelemetryRecordRepository extends JpaRepository<TelemetryRecord, Long> {
    List<TelemetryRecord> findByDevEuiOrderByTimestampDesc(String devEui);
    List<TelemetryRecord> findByDevEuiOrderByTimestampDesc(String devEui, Pageable pageable);
    Optional<TelemetryRecord> findTopByDevEuiOrderByTimestampDesc(String devEui);
    List<TelemetryRecord> findByDevEuiAndTimestampAfterOrderByTimestampAsc(String devEui, Instant since);
}
