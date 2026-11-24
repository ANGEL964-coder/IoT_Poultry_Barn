package com.sdn.poultry.security;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Service;

import java.util.ArrayDeque;
import java.util.Deque;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentMap;
import java.util.concurrent.atomic.AtomicBoolean;

@Service
public class RateLimiterService {

    @Value("${app.security.rate-limit.max-requests:120}")
    private int maxRequests = 120;

    @Value("${app.security.rate-limit.window-seconds:60}")
    private int windowSeconds = 60;

    private final ConcurrentMap<String, Deque<Long>> clientRequestMap = new ConcurrentHashMap<>();

    public RateLimiterService() {
    }

    public RateLimiterService(int maxRequests, int windowSeconds) {
        this.maxRequests = maxRequests;
        this.windowSeconds = windowSeconds;
    }

    /**
     * Atomically evaluates whether clientIp is permitted to execute a request in the sliding window.
     * Returns true if allowed, false if limit exceeded.
     */
    public boolean tryAcquire(String clientIp) {
        if (clientIp == null || clientIp.isBlank()) {
            clientIp = "127.0.0.1";
        }
        long now = System.currentTimeMillis();
        long cutoff = now - (windowSeconds * 1000L);

        AtomicBoolean allowed = new AtomicBoolean(false);
        clientRequestMap.compute(clientIp, (ip, timestamps) -> {
            if (timestamps == null) {
                timestamps = new ArrayDeque<>();
            }
            while (!timestamps.isEmpty() && timestamps.peekFirst() <= cutoff) {
                timestamps.pollFirst();
            }
            if (timestamps.size() < maxRequests) {
                timestamps.addLast(now);
                allowed.set(true);
            } else {
                allowed.set(false);
            }
            return timestamps;
        });
        return allowed.get();
    }

    public int getRequestCount(String clientIp) {
        if (clientIp == null || clientIp.isBlank()) {
            clientIp = "127.0.0.1";
        }
        Deque<Long> deque = clientRequestMap.get(clientIp);
        if (deque == null) {
            return 0;
        }
        long cutoff = System.currentTimeMillis() - (windowSeconds * 1000L);
        synchronized (deque) {
            return (int) deque.stream().filter(t -> t > cutoff).count();
        }
    }

    public void reset(String clientIp) {
        if (clientIp != null) {
            clientRequestMap.remove(clientIp);
        }
    }

    public void resetAll() {
        clientRequestMap.clear();
    }

    public int getMaxRequests() {
        return maxRequests;
    }

    public int getWindowSeconds() {
        return windowSeconds;
    }

    @Scheduled(fixedRateString = "${app.security.rate-limit.cleanup-ms:60000}")
    public void purgeStaleRecords() {
        long cutoff = System.currentTimeMillis() - (windowSeconds * 1000L);
        clientRequestMap.entrySet().removeIf(entry -> {
            Deque<Long> deque = entry.getValue();
            synchronized (deque) {
                while (!deque.isEmpty() && deque.peekFirst() <= cutoff) {
                    deque.pollFirst();
                }
                return deque.isEmpty();
            }
        });
    }
}
