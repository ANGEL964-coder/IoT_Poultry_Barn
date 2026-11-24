## 2026-11-15T22:43:04Z
You are a Read-Only Exploration Agent (teamwork_explorer).
Your Role: RTC ADC Explorer for Milestone 1 (Battery Firmware).
Working directory: c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot
Your dedicated state folder: c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\teamwork_explorer_m1_3\

MANDATORY FIRST STEP:
Read ORIGINAL_REQUEST.md at:
c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\ORIGINAL_REQUEST.md
Also read PROJECT.md at:
c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\PROJECT.md
and SCOPE.md at:
c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\sub_orch_m1\SCOPE.md

TARGET CODE:
Inspect c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\heltec_v3_local_battery\heltec_v3_local_battery.ino
and survey handoffs:
c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\teamwork_explorer_survey_1\handoff.md
c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\teamwork_explorer_survey_3\handoff.md

OBJECTIVE:
Analyze and formulate the exact code modifications for Features F9, F10, F11, F12 (Internal RTC, LoRaWAN Time Sync & ADC Sampling):
1. ESP32-S3 Internal RTC (F9): Inclusion of <time.h> and <sys/time.h>, time formatting helper obtenerHoraFormateada(char* buf, size_t len) ("HH:MM:SS" or "--:--:--"), and timestamp setting via settimeofday().
2. LoRaWAN DeviceTimeReq sync (F10): Queue node.sendMacCommandReq(RADIOLIB_LORAWAN_MAC_DEVICE_TIME) on OTAA join and periodic uplink, parse downlink using node.getMacDeviceTimeAns(&epoch, &fracMs, true), set internal RTC, and set rtc_sincronizado flag.
3. ADC On-Demand Power Saver (F11): Decouple ADC reading (actualizarBalanza() and actualizarBateria()) from unconditional 300ms loop. Implement on-demand sampling during LoRa transmission (Solo en TX) or when scheduled interval elapses.
4. Configurable ADC Sampling Menu (F12): Implement ModoMuestreoAdc enum (0=Solo en TX, 1=10s, 2=30s, 3=60s, 4=300s), NVS persistence under namespace "node_cfg", key "adc_idx", default to 1 (10s) or 0 (Solo en TX), and submenu integration.
5. Strict zero emojis policy compliance (F14).

Produce a comprehensive handoff report at:
c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\teamwork_explorer_m1_3\handoff.md
Follow the standard Handoff format (Observation, Logic Chain, Caveats, Conclusion, Verification Method).
When complete, notify me with your report path.
