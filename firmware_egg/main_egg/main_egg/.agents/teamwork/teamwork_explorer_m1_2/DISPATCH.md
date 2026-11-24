## 2026-11-15T22:42:57Z
You are a Read-Only Exploration Agent (teamwork_explorer).
Your Role: OLED UI Explorer for Milestone 1 (Battery Firmware).
Working directory: c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot
Your dedicated state folder: c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\teamwork_explorer_m1_2\

MANDATORY FIRST STEP:
Read ORIGINAL_REQUEST.md at:
c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\ORIGINAL_REQUEST.md
Also read PROJECT.md at:
c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\PROJECT.md
and SCOPE.md at:
c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\sub_orch_m1\SCOPE.md

TARGET CODE:
Inspect c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\heltec_v3_local_battery\heltec_v3_local_battery.ino
and survey handoff:
c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\teamwork_explorer_survey_2\handoff.md

OBJECTIVE:
Analyze and formulate the exact code modifications for Features F5, F6, F7, F8 (OLED UI Redesign & Modal Overhaul):
1. Clumsy footer removal (F5): Identify and remove "[Mantener PRG: Menu]" and all static footers across all 5 screens (renderPantallaHome, renderPantallaBalanza, renderPantallaBateria, renderPantallaComms, renderPantallaSdn).
2. Screen layout refactoring (F6): Eliminate text overflows (> 21 characters on 128px screen), e.g. "Bruto:%.2fkg V:%.3fV", "ToA: %.1fms SF%u @ %ddBm", "CMD : 0x%02X (%s)", splash screen offsets.
3. Technical Header Bar (F7): Design and implement standard 10px header bar (y=0..9, separator line at y=9) displaying 4-character screen name (HOME, BALA, BATT, COMM, SDN), RTC clock HH:MM:SS, LoRa sync flag (* or -), and 5 progress indicator dots.
4. Dynamic Submenu Modal (F8): Overhaul renderSubmenu() and cursor navigation to support dynamic totalOpciones up to 5 items without overflow or cursor modulo wrap bugs ((submenuCursor + 1) % totalItems).
5. Strict zero emojis policy compliance (F14).

Produce a comprehensive handoff report at:
c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\teamwork_explorer_m1_2\handoff.md
Follow the standard Handoff format (Observation, Logic Chain, Caveats, Conclusion, Verification Method).
When complete, notify me with your report path.
