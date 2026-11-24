package com.sdn.poultry.repository;

import com.sdn.poultry.entity.SdnRuleEvent;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.stereotype.Repository;

import java.util.List;

@Repository
public interface SdnRuleEventRepository extends JpaRepository<SdnRuleEvent, Long> {
    List<SdnRuleEvent> findByDevEuiOrderBySentAtDesc(String devEui);
    List<SdnRuleEvent> findTop50ByOrderBySentAtDesc();
}
