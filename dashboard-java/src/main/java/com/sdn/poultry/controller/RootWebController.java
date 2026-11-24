package com.sdn.poultry.controller;

import org.springframework.core.io.ClassPathResource;
import org.springframework.http.MediaType;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RestController;

import java.io.File;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

@RestController
public class RootWebController {

    private static final String DEV_PATH = "/home/server/CONTROLADOR_SDN/dashboard-java/src/main/resources/static/index.html";

    @GetMapping(value = {"/", "/index.html"}, produces = {"text/html", "application/json", "*/*"})
    public ResponseEntity<String> getRootHtml() {
        // 1. Try reading directly from filesystem for instantaneous live updates
        try {
            File fsFile = new File(DEV_PATH);
            if (fsFile.exists() && fsFile.canRead()) {
                String html = Files.readString(fsFile.toPath(), StandardCharsets.UTF_8);
                return ResponseEntity.ok()
                        .contentType(MediaType.parseMediaType("text/html;charset=UTF-8"))
                        .body(html);
            }
        } catch (Exception ignored) {
        }

        // 2. Fallback to classpath resource bundled in JAR
        try (InputStream in = new ClassPathResource("static/index.html").getInputStream()) {
            String html = new String(in.readAllBytes(), StandardCharsets.UTF_8);
            return ResponseEntity.ok()
                    .contentType(MediaType.parseMediaType("text/html;charset=UTF-8"))
                    .body(html);
        } catch (Exception e) {
            return ResponseEntity.ok("<!DOCTYPE html><html><head><title>SDN Poultry SCADA</title></head><body><h1>SDN Dashboard</h1></body></html>");
        }
    }
}
