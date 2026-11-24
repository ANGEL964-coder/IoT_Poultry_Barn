package com.sdn.poultry.controller;

import com.sdn.poultry.security.ClientIpResolver;
import com.sdn.poultry.service.SystemDocumentationService;
import jakarta.servlet.http.HttpServletRequest;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.Map;

@RestController
@RequestMapping("/api/docs")
@CrossOrigin(origins = "*")
public class SystemDocumentationController {

    private final SystemDocumentationService documentationService;

    public SystemDocumentationController(SystemDocumentationService documentationService) {
        this.documentationService = documentationService;
    }

    @PostMapping("/generate")
    public ResponseEntity<SystemDocumentationService.DocGenerationResult> generateDocs(
            @RequestParam(name = "forceOffline", defaultValue = "false") boolean forceOffline,
            HttpServletRequest request
    ) {
        String clientIp = ClientIpResolver.resolveClientIp(request);
        SystemDocumentationService.DocGenerationResult result = documentationService.generateDocumentation(forceOffline, clientIp);
        return ResponseEntity.ok(result);
    }

    @GetMapping("/status")
    public ResponseEntity<Map<String, Object>> getDocsStatus() {
        Map<String, Object> status = documentationService.getDocsStatus();
        return ResponseEntity.ok(status);
    }
}
