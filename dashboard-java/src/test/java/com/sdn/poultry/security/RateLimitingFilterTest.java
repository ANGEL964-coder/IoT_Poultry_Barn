package com.sdn.poultry.security;

import com.fasterxml.jackson.databind.ObjectMapper;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import org.springframework.mock.web.MockFilterChain;
import org.springframework.mock.web.MockHttpServletRequest;
import org.springframework.mock.web.MockHttpServletResponse;

import static org.assertj.core.api.Assertions.assertThat;

class RateLimitingFilterTest {

    private RateLimiterService rateLimiterService;
    private RateLimitingFilter filter;
    private final ObjectMapper objectMapper = new ObjectMapper();

    @BeforeEach
    void setUp() {
        rateLimiterService = new RateLimiterService(120, 60);
        filter = new RateLimitingFilter(rateLimiterService, objectMapper);
    }

    @Test
    @DisplayName("Should pass requests through when under 120 req/min limit")
    void testPermittedRequests() throws Exception {
        MockHttpServletRequest request = new MockHttpServletRequest("GET", "/api/status");
        request.setRemoteAddr("192.168.1.55");
        MockHttpServletResponse response = new MockHttpServletResponse();
        MockFilterChain filterChain = new MockFilterChain();

        filter.doFilter(request, response, filterChain);

        assertThat(response.getStatus()).isEqualTo(200);
    }

    @Test
    @DisplayName("Should return HTTP 429 with Retry-After: 60 when rate limit is exceeded")
    void testRateLimitExceededReturns429() throws Exception {
        String clientIp = "192.168.1.66";

        for (int i = 0; i < 120; i++) {
            MockHttpServletRequest req = new MockHttpServletRequest("GET", "/api/status");
            req.setRemoteAddr(clientIp);
            MockHttpServletResponse resp = new MockHttpServletResponse();
            MockFilterChain chain = new MockFilterChain();
            filter.doFilter(req, resp, chain);
            assertThat(resp.getStatus()).isEqualTo(200);
        }

        // 121st request
        MockHttpServletRequest req121 = new MockHttpServletRequest("GET", "/api/status");
        req121.setRemoteAddr(clientIp);
        MockHttpServletResponse resp121 = new MockHttpServletResponse();
        MockFilterChain chain121 = new MockFilterChain();

        filter.doFilter(req121, resp121, chain121);

        assertThat(resp121.getStatus()).isEqualTo(429);
        assertThat(resp121.getHeader("Retry-After")).isEqualTo("60");
        assertThat(resp121.getContentType()).contains("application/json");

        String content = resp121.getContentAsString();
        assertThat(content).contains("Too Many Requests");
        assertThat(content).contains("Rate limit exceeded");
    }

    @Test
    @DisplayName("Should extract client IP from X-Forwarded-For header")
    void testXForwardedForClientIpResolution() throws Exception {
        String spoofedProxy = "10.0.0.1";
        String actualClientIp = "203.0.113.195";

        for (int i = 0; i < 120; i++) {
            rateLimiterService.tryAcquire(actualClientIp);
        }

        MockHttpServletRequest req = new MockHttpServletRequest("GET", "/api/status");
        req.setRemoteAddr(spoofedProxy);
        req.addHeader("X-Forwarded-For", actualClientIp + ", 10.0.0.1");
        MockHttpServletResponse resp = new MockHttpServletResponse();
        MockFilterChain chain = new MockFilterChain();

        filter.doFilter(req, resp, chain);

        assertThat(resp.getStatus()).isEqualTo(429);
    }

    @Test
    @DisplayName("Should bypass OPTIONS preflight CORS requests")
    void testOptionsPreflightBypassed() throws Exception {
        MockHttpServletRequest req = new MockHttpServletRequest("OPTIONS", "/api/sdn/downlink");
        MockHttpServletResponse resp = new MockHttpServletResponse();
        MockFilterChain chain = new MockFilterChain();

        filter.doFilter(req, resp, chain);

        assertThat(resp.getStatus()).isEqualTo(200);
    }
}
