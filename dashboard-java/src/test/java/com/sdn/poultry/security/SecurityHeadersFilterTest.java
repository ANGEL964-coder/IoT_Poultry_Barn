package com.sdn.poultry.security;

import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import org.springframework.mock.web.MockFilterChain;
import org.springframework.mock.web.MockHttpServletRequest;
import org.springframework.mock.web.MockHttpServletResponse;

import static org.assertj.core.api.Assertions.assertThat;

class SecurityHeadersFilterTest {

    private final SecurityHeadersFilter filter = new SecurityHeadersFilter();

    @Test
    @DisplayName("Should inject all mandatory security headers on every response")
    void testSecurityHeadersInjected() throws Exception {
        MockHttpServletRequest request = new MockHttpServletRequest("GET", "/api/status");
        MockHttpServletResponse response = new MockHttpServletResponse();
        MockFilterChain filterChain = new MockFilterChain();

        filter.doFilter(request, response, filterChain);

        assertThat(response.getHeader("X-Content-Type-Options")).isEqualTo("nosniff");
        assertThat(response.getHeader("X-Frame-Options")).isEqualTo("SAMEORIGIN");
        assertThat(response.getHeader("X-XSS-Protection")).isEqualTo("1; mode=block");
        assertThat(response.getHeader("Referrer-Policy")).isEqualTo("strict-origin-when-cross-origin");
        assertThat(response.getHeader("Content-Security-Policy")).isEqualTo("default-src 'self' 'unsafe-inline' 'unsafe-eval' data: https: http:; font-src 'self' data: https:; img-src 'self' data: https: blob:; connect-src 'self' ws: wss: http: https:;");
    }

    @Test
    @DisplayName("Should pass request through filter chain")
    void testFilterChainInvocation() throws Exception {
        MockHttpServletRequest request = new MockHttpServletRequest("POST", "/api/sdn/downlink");
        MockHttpServletResponse response = new MockHttpServletResponse();

        boolean[] chainExecuted = new boolean[]{false};
        MockFilterChain filterChain = new MockFilterChain() {
            @Override
            public void doFilter(jakarta.servlet.ServletRequest req, jakarta.servlet.ServletResponse res) {
                chainExecuted[0] = true;
            }
        };

        filter.doFilter(request, response, filterChain);

        assertThat(chainExecuted[0]).isTrue();
    }
}
