## 2026-11-15T22:43:16Z
Sender: 4f6c07a2-ac4d-43a6-8820-937b2a4b334d
Priority: MESSAGE_PRIORITY_HIGH

You are teamwork_spec_miner for E2E Test Specification Mining (R1 & R2).
Working directory: c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot
Your dedicated state folder: c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\spec_miner_e2e_1\
Parent conversation ID: 4f6c07a2-ac4d-43a6-8820-937b2a4b334d

MANDATORY FIRST STEPS:
1. Read ORIGINAL_REQUEST.md at:
   c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\ORIGINAL_REQUEST.md
2. Read PROJECT.md at:
   c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\PROJECT.md
3. Read the survey handoff reports:
   - c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\teamwork_explorer_survey_1\handoff.md
   - c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\teamwork_explorer_survey_2\handoff.md
   - c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\teamwork_explorer_survey_3\handoff.md

TASK:
Exhaustively specify the test cases, test vectors, boundary conditions, and assertion rules for:
- R1: Debounced Deep Sleep Entry (PRG button release >= 80ms, SX1262 sleep retention, Vext & ADC_CTRL pin holds, ext0 re-arming).
- R2: OLED Menu Redesign (removal of "[Mantener PRG: Menu]" and static footers, string width <= 21 chars / 128px, technical header with RTC, 5-item submenu modal geometry and modulo bounds).

Structure your output into:
- Tier 1: Feature Coverage (>=5 test cases per feature)
- Tier 2: Boundary & Corner Cases (>=5 test cases per feature: button bounce durations, 20/21/22 char strings, etc.)
- Tier 3: Pairwise combinations
- Tier 4: Real-world scenarios

Write your complete report to `handoff.md` in your state folder (`c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\spec_miner_e2e_1\handoff.md`) and notify parent with send_message.
Do NOT modify source code files. You are a read-only specification investigator.
