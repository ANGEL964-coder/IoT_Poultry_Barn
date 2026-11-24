package com.sdn.poultry.security;

import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicInteger;

import static org.assertj.core.api.Assertions.assertThat;

class RateLimiterServiceTest {

    private RateLimiterService rateLimiterService;

    @BeforeEach
    void setUp() {
        rateLimiterService = new RateLimiterService(120, 60);
    }

    @Test
    @DisplayName("Should permit up to 120 requests and reject 121st within sliding window")
    void testRateLimitPermits120AndRejects121st() {
        String clientIp = "192.168.1.100";

        for (int i = 0; i < 120; i++) {
            boolean acquired = rateLimiterService.tryAcquire(clientIp);
            assertThat(acquired).as("Request " + (i + 1) + " should be acquired").isTrue();
        }

        boolean overLimit = rateLimiterService.tryAcquire(clientIp);
        assertThat(overLimit).as("121st request should be rejected").isFalse();
        assertThat(rateLimiterService.getRequestCount(clientIp)).isEqualTo(120);
    }

    @Test
    @DisplayName("Should maintain separate rate limiting buckets for distinct IP addresses")
    void testDistinctIpIsolation() {
        String ip1 = "10.0.0.1";
        String ip2 = "10.0.0.2";

        for (int i = 0; i < 120; i++) {
            assertThat(rateLimiterService.tryAcquire(ip1)).isTrue();
        }
        assertThat(rateLimiterService.tryAcquire(ip1)).isFalse();

        // ip2 should still have full quota
        assertThat(rateLimiterService.tryAcquire(ip2)).isTrue();
        assertThat(rateLimiterService.getRequestCount(ip2)).isEqualTo(1);
    }

    @Test
    @DisplayName("Should reset quota when reset() is called")
    void testResetIp() {
        String ip = "172.16.0.50";
        for (int i = 0; i < 120; i++) {
            rateLimiterService.tryAcquire(ip);
        }
        assertThat(rateLimiterService.tryAcquire(ip)).isFalse();

        rateLimiterService.reset(ip);
        assertThat(rateLimiterService.getRequestCount(ip)).isEqualTo(0);
        assertThat(rateLimiterService.tryAcquire(ip)).isTrue();
    }

    @Test
    @DisplayName("Should handle concurrent requests safely without race conditions")
    void testConcurrentAccess() throws InterruptedException {
        String ip = "192.168.10.1";
        int threadCount = 20;
        int requestsPerThread = 10; // Total 200 requests attempted
        ExecutorService executor = Executors.newFixedThreadPool(threadCount);
        CountDownLatch latch = new CountDownLatch(threadCount);
        AtomicInteger successCount = new AtomicInteger(0);
        AtomicInteger rejectedCount = new AtomicInteger(0);

        for (int i = 0; i < threadCount; i++) {
            executor.submit(() -> {
                try {
                    for (int j = 0; j < requestsPerThread; j++) {
                        if (rateLimiterService.tryAcquire(ip)) {
                            successCount.incrementAndGet();
                        } else {
                            rejectedCount.incrementAndGet();
                        }
                    }
                } finally {
                    latch.countDown();
                }
            });
        }

        latch.await();
        executor.shutdown();

        assertThat(successCount.get()).isEqualTo(120);
        assertThat(rejectedCount.get()).isEqualTo(80);
    }
}
