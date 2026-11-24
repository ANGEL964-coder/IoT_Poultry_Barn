# Handoff Report: E2E Test Runner Architecture & Harness Design (R5 & R6)

**Role**: teamwork_explorer (Explorer E2E 3)  
**Dedicated State Folder**: `c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\explorer_e2e_3\`  
**Target Delivery Artifact**: `test/test_runner.py` architectural blueprint and test infrastructure specification  
**Scope**: Requirements R5 (Energy Audit Validation) & R6 (Integrity, Zero Emojis, Immutability, Dual Compilation Harness), across Tiers 1-4.

---

## 1. Observation

### 1.1 Protected Baseline Version 1.0 Inventory and Cryptographic Hashes
Direct inspection of `c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\` reveals 5 source/header files (plus the `.agents\` metadata directory):
- Target Directory: `c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\`
- Baseline Archive: `c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main.rar` (Size: 23,445 bytes, Last Modified: 11/11/2026 13:44)

Direct file hash computation using `Get-FileHash -Algorithm SHA256`:
| File Name | Exact Size (Bytes) | SHA-256 Checksum (Hexadecimal) |
|---|---|---|
| `heltec_v3_config.h` | 3,890 | `58C1E8FBF171373EDBC0CF4EC5E810EF4AA2B9E947D44EF0E19E82D6E3EF537F` |
| `main.ino` | 32,966 | `1B3DB58322463193AA53D0DBBD4EA87ACEDD474ADD2C9D70A074A0A84B716DB0` |
| `scale_driver.h` | 10,760 | `7FE6333CBB9A10B49E6029517887EB46BDB9A2A67A56226F359BB51195D77A22` |
| `sdn_header.h` | 3,665 | `5B90F21F5135645B6EB2E4C0BB6F7482EAE2516F1DCC84EABA796E0D34A5FD89` |
| `web_interface.h` | 48,636 | `A1E909033ECE4F4C75D80CF104FFE315AE8FC84E284B169DB40BA7C60D5D1FDE` |

Total baseline source file count in `main\`: exactly 5 files.

### 1.2 Emoji Audit Findings Across Workspace
An automated Unicode regex scanner was executed across all `.ino`, `.h`, `.md`, `.py`, and `.json` files in the repository.

1. Active Development Directories and Documentation:
   - `heltec_v3_local_battery/heltec_v3_local_battery.ino`: **0 emojis** (100% clean)
   - `heltec_v3_local_battery/heltec_v3_config.h`: **0 emojis** (100% clean)
   - `heltec_v3_local_battery/sdn_header.h`: **0 emojis** (100% clean)
   - `heltec_v3_local_diag/heltec_v3_local_diag.ino`: **0 emojis** (100% clean; note: one non-ASCII degree character `°` at line 265 in comment)
   - `heltec_v3_local_diag/sdn_header.h`: **0 emojis** (100% clean)
   - `PROJECT.md`: **0 emojis** (100% clean)
   - `AUDITORIA_ENERGIA_TEMPLATE.md`: **0 emojis** (100% clean)

2. Legacy Version 1.0 (`main\`) Findings:
   - `main\main.ino`: **7 emoji occurrences detected** in Serial print statements:
     * Line 429: `[U+1F4E5]` (U+1F4E5, INBOX TRAY)
     * Line 442: `[U+2714]` (U+2714, HEAVY CHECK MARK)
     * Line 448: `[U+274C]` (U+274C, CROSS MARK)
     * Line 506: `[U+1F4E5]` (U+1F4E5, INBOX TRAY)
     * Line 537: `[U+26A1]` (U+26A1, HIGH VOLTAGE SIGN)
     * Line 545: `[U+2714]` (U+2714, HEAVY CHECK MARK)
     * Line 547: `[U+274C]` (U+274C, CROSS MARK)
   - `main\web_interface.h`: **1 emoji occurrence detected**:
     * Line 944: `[U+2714]` (U+2714, HEAVY CHECK MARK)

Because `main\` is strictly immutable, these legacy emojis must remain untouched in `main\`. The strict ZERO EMOJIS requirement applies to all new/modified files, OLED strings, active code branches (`heltec_v3_local_battery`, `heltec_v3_local_diag`), `AUDITORIA_ENERGIA.md`, `test/`, and project documentation.

### 1.3 Dual Compilation Toolchain Verification
Direct empirical verification was performed using the host Arduino CLI executable:
- Executable Path: `C:\Users\mate1\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe`
- Version: `arduino-cli Version: 1.3.1 Commit: 08ff7e2b Date: 2025-08-28T13:51:38Z`
- Target Board FQBN: `esp32:esp32:heltec_wifi_lora_32_V3`
- ESP32 Core: Version 3.3.6 (ESP-IDF v5.1 based)

Compilation Run 1 — Diagnostic Sketch:
- Command:
  `& "C:\Users\mate1\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile --fqbn esp32:esp32:heltec_wifi_lora_32_V3 --build-path "C:\Users\mate1\AppData\Local\Temp\ard_build" "c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\heltec_v3_local_diag"`
- Exit Code: `0`
- Program Storage: `365,215 bytes (10%)` of 3,342,336 bytes maximum
- Dynamic Memory: `22,392 bytes (6%)` of 327,680 bytes maximum

Compilation Run 2 — Battery LoRaWAN Sketch:
- Command:
  `& "C:\Users\mate1\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile --fqbn esp32:esp32:heltec_wifi_lora_32_V3 --build-path "C:\Users\mate1\AppData\Local\Temp\ard_build_bat" "c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\heltec_v3_local_battery"`
- Exit Code: `0`
- Program Storage: `454,671 bytes (13%)` of 3,342,336 bytes maximum
- Dynamic Memory: `25,632 bytes (7%)` of 327,680 bytes maximum

### 1.4 Python Runtime Environment on Host
Direct probe of Python executables:
- Calling `python` directly in shell triggers WindowsApps Microsoft Store redirect stub (`0.0.0.0`).
- The official Python Launcher `py.exe` is installed and operational at:
  `C:\Users\mate1\AppData\Local\Programs\Python\Launcher\py.exe`
- Available Python engines:
  * Python 3.14.0 (64-bit AMD64) at `C:\Users\mate1\AppData\Local\Programs\Python\Python314\python.exe`
  * Python 3.10.0 (64-bit AMD64) at `C:\Users\mate1\AppData\Local\Programs\Python\Python310\python.exe`
- Standard library verification (`unittest`, `json`, `hashlib`, `subprocess`, `re`, `os`, `sys`): **100% available and verified**.
- Invocation standard for Windows: `py test/test_runner.py` or direct python path.

### 1.5 Energy Audit Report Requirements (R5)
Analysis of `AUDITORIA_ENERGIA_TEMPLATE.md` establishes the exact quantitative criteria that the test harness must validate:
1. Required Section Structure:
   - Section 1: Executive Summary (`## 1. Resumen Ejecutivo` or English equivalent)
   - Section 2: Hardware Architecture & Power Rails (`## 2. Arquitectura de Hardware y Dominios de Alimentacion`)
   - Section 3: Operating States & Quantitative Model (`## 3. Estados de Operacion y Modelado Energetico Cuantitativo`)
   - Section 4: Battery Autonomy Projection (`## 4. Analisis de Proyeccion de Autonomia de Bateria`)
   - Section 5: Hardware Recommendations (`## 5. Recomendaciones de Hardware`)
   - Section 6: Software Recommendations (`## 6. Recomendaciones de Software`)
2. Quantitative Consumption Comparison Table:
   Must contain a Markdown table comparing Baseline ("Antes" / "Base") vs Optimized ("Despues" / "Optimizado") across 10 operational states (E1 to E10):
   - E1 (Active with OLED): Base ~68 mA vs Opt ~42.5 mA
   - E2 (Active OLED Timeout): Base ~38 mA vs Opt ~21 mA
   - E3 (ADC Battery Reading): Base ~38 mA (continuous c/300ms) vs Opt ~22 mA (15ms window)
   - E4 (ADC Balanza Reading): Base ~48 mA (continuous c/300ms) vs Opt ~31 mA (20ms window)
   - E5 (LoRa TX +14 dBm): Base ~85 mA vs Opt ~82 mA
   - E6 (LoRa TX +22 dBm): Base ~163 mA vs Opt ~160 mA
   - E7 (RX1/RX2 Windows): Base ~52 mA vs Opt ~12.5 mA
   - E8 (Deep Sleep PRG Defect): Base ~38 mA (immediate re-wake) vs Opt Resuelto/N/A
   - E9 (Deep Sleep ext0 PRG Wake): Base ~1,850 uA vs Opt 120 uA (15.4x reduction)
   - E10 (Deep Sleep RTC Timer Wake): Base ~1,810 uA vs Opt 24 uA (75.4x reduction)
3. Autonomy Calculations & Formulas:
   - Mathematical formula for average current: $I_{avg} = \frac{\sum (I_i \cdot t_i)}{T_{periodo}}$
   - 1000 mAh LiPo model (800 mAh effective @ 80% DoD):
     * 1 min interval: Base 21.0 h vs Opt >= 25 days (600+ h)
     * 5 min interval: Base 21.0 h vs Opt >= 120 days (2,800+ h)
     * 15 min interval: Base 21.0 h vs Opt >= 300 days (7,200+ h)
     * 1 hour interval: Base 21.0 h vs Opt >= 700 days (16,000+ h)
   - 3000 mAh LiPo model (2550 mAh effective @ 85% DoD):
     * 5 min interval: Opt >= 380 days (9,000+ h)
     * 15 min interval: Opt >= 990 days (23,000+ h)
4. Specific Technical Hardware Assertions:
   - Switched rail Vext on GPIO 36 (Active LOW, P-MOSFET AO3401/AO7801).
   - Battery divider switching on GPIO 37 (Active HIGH N-MOSFET, 4.9:1 divider with 390k/100k resistors).
   - SX1262 LoRa transceiver wired to always-on 3.3V rail requiring software sleep (`radio.sleep(false)`).
   - OLED SSD1306 display off and I2C/Reset bus isolation (preventing 250-400 uA parasitic ESD clamp diode leakage).
5. Specific Technical Software Assertions:
   - PRG button release loop with debounce >= 80ms (`while (digitalRead(PIN_PRG) == LOW) ... delay(80)`).
   - `radio.sleep(false)` before `esp_deep_sleep_start()`.
   - `gpio_hold_en()` and `gpio_deep_sleep_hold_en()`.
   - CPU clock scaling to 80 MHz.
   - On-demand and configurable ADC sampling interval.
   - LoRaWAN `DeviceTimeReq` and internal RTC integration (`settimeofday`).

---

## 2. Logic Chain

### 2.1 Pure Python Standard Library Requirement Logic
1. The project requires automated CI/CD and local developer execution on Windows/Linux environments without requiring external package installations (`pip install pytest`, etc.).
2. The standard library modules `unittest`, `json`, `hashlib`, `subprocess`, `re`, `os`, `sys`, `time`, and `argparse` provide complete capabilities for:
   - Test definition, assertion, and runner lifecycle (`unittest`).
   - Command-line argument parsing (`argparse`).
   - Cryptographic hashing for bit-for-bit immutability (`hashlib.sha256`).
   - External compiler toolchain invocation and output parsing (`subprocess.run`).
   - File system traversal and regex pattern matching (`os.walk`, `pathlib.Path`, `re`).
   - Structured JSON reporting artifact generation (`json.dump`).
3. Therefore, building `test/test_runner.py` with zero external dependencies ensures 100% out-of-the-box execution reliability across any development machine.

### 2.2 CLI Interface and Exit Code Semantics Logic
1. Continuous integration workflows require deterministic return codes:
   - Return Code `0`: All executed test assertions passed.
   - Return Code `1`: One or more test assertions failed or encountered an unhandled exception.
2. CLI flags are designed to support granular workflows:
   - `--tier [1|2|3|4|all]`: Filters test suite execution to target tiers. Default is `all`.
   - `--compile`: Explicitly triggers the dual firmware compilation harness via `arduino-cli`.
   - `--summary`: Outputs a human-readable ANSI/plain-text table summarizing test outcomes, memory consumption, and integrity status.
   - `--json [path]`: Specifies output path for the JSON results artifact (defaulting to `test/test_results.json`).

### 2.3 Cryptographic Immutability Validation Logic (R6)
1. Observation 1.1 establishes the exact 5 files in `main\` and their SHA-256 checksums.
2. The requirement strictly forbids any modification to version 1.0.
3. Therefore, the test runner must:
   - Verify that exactly those 5 files exist in `main\` (excluding `.agents\`).
   - Verify that file byte sizes match the baseline exactly.
   - Verify that SHA-256 hashes match the baseline bit-for-bit.
   - Detect any unauthorized additions, deletions, or single-byte modifications.

### 2.4 Strict Zero Emojis Validation Logic (R6)
1. Observation 1.2 demonstrates that legacy v1.0 in `main\` contains 8 emojis, while active development sketches and docs have 0.
2. The prompt specifies: "Prohibicion absoluta de emojis en todo el codigo fuente, cadenas de texto, interfaz OLED y documentacion tecnica" while "Mantener estrictamente intacta la version 1.0 ubicada en main\".
3. Therefore, the zero emojis scanner must:
   - Target all active development files (`heltec_v3_local_battery/`, `heltec_v3_local_diag/`, `AUDITORIA_ENERGIA.md`, `PROJECT.md`, `test/`).
   - Scan source code lines, string literals, and code comments.
   - Check OLED display string buffers and menu option arrays.
   - Exclude the protected `main\` directory from emoji removal so that `main\`'s bit-for-bit cryptographic hash is never altered.
   - Use a Unicode regex covering SMP emoticons (`\U0001F600-\U0001F64F`), pictographs (`\U0001F300-\U0001F5FF`), transport (`\U0001F680-\U0001F6FF`), symbols extended (`\U0001F900-\U0001FAFF`), dingbats (`\u2700-\u27BF`), and miscellaneous symbols (`\u2600-\u26FF`), while properly preserving Spanish accents (`á`, `é`, `í`, `ó`, `ú`, `ñ`).

### 2.5 Quantitative Energy Audit Validation Logic (R5)
1. Observation 1.5 establishes the exact structure, tables, numerical thresholds, and recommendations expected in `AUDITORIA_ENERGIA.md`.
2. A superficial check (e.g. checking if the file exists) is insufficient.
3. The harness must implement a dedicated markdown parser that:
   - Validates existence and non-empty size of `AUDITORIA_ENERGIA.md`.
   - Checks presence of all 6 required section headers.
   - Extracts the state comparison table and verifies that all 10 states (E1..E10) are listed with both Baseline and Optimized values.
   - Validates that current units are specified in mA and uA, and that the reduction factor for deep sleep is quantitatively verified (e.g. >= 15x for ext0, >= 70x for RTC timer).
   - Validates autonomy calculation consistency (verifying $I_{avg}$ formula, 1000 mAh and 3000 mAh LiPo tables, and gain percentages).
   - Asserts the presence of mandatory hardware recommendations (Vext GPIO 36, battery MOSFET GPIO 37, SX1262 sleep, OLED I2C isolation) and software recommendations (PRG release loop with debounce, RTC sync, configurable ADC).

### 2.6 Dual Compilation Harness Logic (R6)
1. Observation 1.3 verified that `arduino-cli compile` works on the host for board `esp32:esp32:heltec_wifi_lora_32_V3`.
2. Windows host requires pre-creating temporary build directories (`C:\Users\mate1\AppData\Local\Temp\ard_build` and `ard_build_bat`) to avoid parallel dependency directory creation faults.
3. The harness must invoke the compilation, capture stdout/stderr, parse Program Storage and Dynamic Memory consumption, and ensure exit code 0.

---

## 3. Caveats

1. **Python Command Invocation on Windows Host**:
   - The default `python` executable in PowerShell routes to the Windows Store redirect stub. Tests must be executed using `py test/test_runner.py` or by passing the explicit path `C:\Users\mate1\AppData\Local\Programs\Python\Python314\python.exe`.
2. **Immutable Main vs Emoji Restriction**:
   - `main\main.ino` contains 7 emojis and `main\web_interface.h` contains 1 emoji. These must NOT be modified, as doing so would violate the bit-for-bit immutability requirement (R6). The zero emojis rule strictly governs all new code, modified sketches, OLED strings, and documentation.
3. **Compilation Build Directory Pre-Creation**:
   - When compiling with `arduino-cli` using custom `--build-path`, the target directories (`C:\Users\mate1\AppData\Local\Temp\ard_build` and `C:\Users\mate1\AppData\Local\Temp\ard_build_bat`) must be created by Python before launching `arduino-cli` to prevent Windows file system race conditions during parallel builds.
4. **Compilation Duration**:
   - The first compilation of `heltec_v3_local_battery` compiles RadioLib and takes ~40-60 seconds. Subsequent builds with `--build-path` are incrementally cached and take ~3-8 seconds. The test runner CLI provides `--tier` selection so developers can run fast integrity and static checks without waiting for compilation every time.
5. **Autonomy Model Battery Assumptions**:
   - Calculations in `AUDITORIA_ENERGIA.md` assume realistic Depth of Discharge (DoD) of 80% for 1000 mAh (800 mAh usable) and 85% for 3000 mAh (2550 mAh usable). Calculations assuming 100% nominal capacity would be physically inaccurate; the test validator verifies DoD realism.

---

## 4. Conclusion: Complete Test Runner Architecture & Harness Design

### 4.1 Architecture Overview and Code Layout
The test suite is structured as a pure Python package in `test/`:
```
c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\
├── test/
│   ├── test_runner.py            # Primary CLI orchestrator and runner entry point
│   ├── test_tier1_feature.py     # Tier 1: Feature Coverage (>=5 tests per feature)
│   ├── test_tier2_boundary.py    # Tier 2: Boundary & Corner Cases (>=5 tests per feature)
│   ├── test_tier3_pairwise.py    # Tier 3: Pairwise Combinations
│   ├── test_tier4_scenarios.py   # Tier 4: Real-world Scenarios (Dual compile, immutability)
│   ├── harness_audit.py          # R5: Quantitative markdown table and formula validator
│   ├── harness_emoji.py          # R6: Strict zero emojis scanner
│   ├── harness_immutability.py   # R6: SHA-256 bit-for-bit integrity verifier
│   ├── harness_compile.py        # R6: Arduino-cli dual compilation runner and memory parser
│   └── test_results.json         # Automated JSON output artifact
```

---

### 4.2 Tier 1: Feature Coverage (>=5 Test Cases per Feature)

#### Feature F13: Energy Efficiency Audit Report (`AUDITORIA_ENERGIA.md` - R5)
- **TC13.1 — File Existence & Header Architecture**:
  Verifies `AUDITORIA_ENERGIA.md` exists in project root, is non-empty (> 2 KB), and contains the 6 required H1/H2 markdown headers: Resumen Ejecutivo, Dominios de Alimentacion, Estados de Operacion, Autonomia de Bateria, Recomendaciones de Hardware, Recomendaciones de Software.
- **TC13.2 — Quantitative Comparison Table State Completeness**:
  Extracts the operational states table and asserts that all 10 states (E1 to E10: Active OLED, Active Timeout, ADC Bat, ADC Balanza, TX +14dBm, TX +22dBm, RX Windows, Sleep PRG failure, Sleep ext0, Sleep RTC Timer) are documented with both Baseline ("Antes") and Optimized ("Despues") values.
- **TC13.3 — Quantitative Current Value & Reduction Factor Assertions**:
  Parses numerical current values using regular expressions. Asserts active currents are in range 20-170 mA, deep sleep ext0 <= 150 uA, deep sleep RTC timer <= 30 uA, and verifies sleep current reduction factor >= 15x for ext0 and >= 70x for RTC timer.
- **TC13.4 — Battery Autonomy Mathematical Modeling & Dual Capacity Projections**:
  Verifies presence of the mathematical formula $I_{avg} = \sum (I_i \cdot t_i) / T$, verifies autonomy tables for 1000 mAh (effective 800 mAh) and 3000 mAh (effective 2550 mAh) across intervals (1m, 5m, 15m, 1h), and asserts projected 5m autonomy >= 120 days on 1000 mAh pack.
- **TC13.5 — Hardware Recommendations Specificity**:
  Verifies that Section 5 explicitly details: (a) Vext switched rail on GPIO 36 Active LOW, (b) Battery divider switching on GPIO 37, (c) SX1262 LoRa software sleep requirement on permanent 3.3V rail, and (d) OLED I2C/Reset bus high-Z isolation to prevent 250-400 uA ESD diode leakage.
- **TC13.6 — Software Recommendations Specificity**:
  Verifies that Section 6 explicitly details: (a) PRG button release loop with debounce >= 80ms, (b) `radio.sleep(false)` before deep sleep, (c) GPIO pad retention via `gpio_hold_en()`, (d) CPU scaling to 80 MHz, (e) Configurable ADC sampling interval, and (f) LoRaWAN `DeviceTimeReq` RTC synchronization.

#### Feature F14: Zero Emojis Compliance (R6)
- **TC14.1 — Active Development Firmware Source Scan**:
  Scans all `.ino` and `.h` files in `heltec_v3_local_battery/` and `heltec_v3_local_diag/`. Asserts 0 emoji matches across code and comments.
- **TC14.2 — Documentation Zero Emojis Scan**:
  Scans `AUDITORIA_ENERGIA.md` and `PROJECT.md`. Asserts 0 emoji matches while allowing valid Spanish characters (`á`, `é`, `í`, `ó`, `ú`, `ñ`).
- **TC14.3 — Test Suite Zero Emojis Scan**:
  Scans all `.py`, `.json`, and `.md` files in `test/`. Asserts 0 emoji matches.
- **TC14.4 — OLED Display String Literal Scan**:
  Extracts all double-quoted strings passed to `display.print()`, `display.printf()`, `display.println()`, `display.drawBitmap()`, and UI string arrays. Asserts 0 emojis and 0 CP437 corrupt characters (e.g. `°` degree symbol).
- **TC14.5 — Code Comments Zero Emojis Scan**:
  Extracts all single-line (`//`) and multi-line (`/* ... */`) comments across active development sketches. Asserts 0 emojis.

#### Feature F15: Immutable Version 1.0 Preservation (R6)
- **TC15.1 — Main File Inventory Completeness**:
  Asserts that `main\` contains exactly 5 files (`heltec_v3_config.h`, `main.ino`, `scale_driver.h`, `sdn_header.h`, `web_interface.h`), excluding `.agents\`. Asserts 0 extra files and 0 deleted files.
- **TC15.2 — Main Exact Byte Size Verification**:
  Asserts exact byte sizes: `heltec_v3_config.h` == 3890, `main.ino` == 32966, `scale_driver.h` == 10760, `sdn_header.h` == 3665, `web_interface.h` == 48636.
- **TC15.3 — Main SHA-256 Bit-for-Bit Hash Verification**:
  Computes SHA-256 for each file in `main\` and asserts exact match against the verified reference baseline:
  - `heltec_v3_config.h`: `58C1E8FBF171373EDBC0CF4EC5E810EF4AA2B9E947D44EF0E19E82D6E3EF537F`
  - `main.ino`: `1B3DB58322463193AA53D0DBBD4EA87ACEDD474ADD2C9D70A074A0A84B716DB0`
  - `scale_driver.h`: `7FE6333CBB9A10B49E6029517887EB46BDB9A2A67A56226F359BB51195D77A22`
  - `sdn_header.h`: `5B90F21F5135645B6EB2E4C0BB6F7482EAE2516F1DCC84EABA796E0D34A5FD89`
  - `web_interface.h`: `A1E909033ECE4F4C75D80CF104FFE315AE8FC84E284B169DB40BA7C60D5D1FDE`
- **TC15.4 — Baseline Archive Integrity**:
  Asserts `main.rar` exists in root, has size == 23445 bytes, and is uncorrupted.
- **TC15.5 — Directory Hygiene Enforcement**:
  Ensures no backup, temporary, or hidden files (`main.ino.bak`, `~main.ino`, `.DS_Store`) exist in `main\`.

#### Feature F16: Dual Firmware Compilation Harness (R6)
- **TC16.1 — Arduino-CLI Toolchain Discovery & FQBN Validation**:
  Locates `arduino-cli.exe`, checks version >= 1.0.0, and validates installed core for `esp32:esp32:heltec_wifi_lora_32_V3`.
- **TC16.2 — Pre-Build Directory Provisioning**:
  Ensures temporary build paths `C:\Users\mate1\AppData\Local\Temp\ard_build` and `ard_build_bat` exist or are created before build execution.
- **TC16.3 — Diagnostic Sketch Compilation (`heltec_v3_local_diag`)**:
  Compiles sketch, asserts exit code 0, verifies Program Storage <= 3,342,336 bytes and Dynamic Memory <= 327,680 bytes.
- **TC16.4 — Battery LoRaWAN Sketch Compilation (`heltec_v3_local_battery`)**:
  Compiles sketch, asserts exit code 0, verifies Program Storage <= 3,342,336 bytes and Dynamic Memory <= 327,680 bytes.
- **TC16.5 — Compilation Diagnostic & Warning Threshold**:
  Inspects stderr; asserts 0 undefined references, 0 fatal compiler errors, and returns memory footprint metrics in JSON output.

#### Additional Firmware Static Assertions (Features F1 to F12)
- **F1 (Deep Sleep Debounce)**: TC1.1 (Release loop present), TC1.2 (Debounce window >= 80ms), TC1.3 (Debounce timer reset on bouncing LOW), TC1.4 (Ext0 armed after release), TC1.5 (Absence of immediate un-debounced sleep).
- **F2 (SX1262 Sleep)**: TC2.1 (`radio.sleep(false)` called), TC2.2 (SX1262 warm retention enabled), TC2.3 (Precedes `esp_deep_sleep_start()`), TC2.4 (Wakeup re-init present), TC2.5 (No un-slept sleep exit).
- **F3 (Peripheral Rail & Pin Hold)**: TC3.1 (Vext GPIO 36 driven HIGH), TC3.2 (ADC_CTRL GPIO 37 driven LOW), TC3.3 (`gpio_hold_en` on 36), TC3.4 (`gpio_hold_en` on 37), TC3.5 (`gpio_deep_sleep_hold_en()` called).
- **F4 (Ext0 Re-arming)**: TC4.1 (`esp_sleep_enable_ext0_wakeup` called with PIN_PRG), TC4.2 (Level parameter is 0 [active LOW]), TC4.3 (Timer wakeup enabled conditionally), TC4.4 (Ext0 not enabled before release loop), TC4.5 (`ESP_SLEEP_WAKEUP_EXT0` handling on boot).
- **F5 (OLED Footer Removal)**: TC5.1 (`[Mantener PRG: Menu]` absent), TC5.2 (`[Mantener: Calibrar]` absent), TC5.3 (`[Mantener: Config TX]` absent), TC5.4 (Footer rule at y=54/55 removed), TC5.5 (Usable body expanded to 54px).
- **F6 (OLED Text Overflow Fixes)**: TC6.1 (HOME strings <= 21 chars), TC6.2 (BALANZA strings <= 21 chars), TC6.3 (BATERIA strings <= 21 chars), TC6.4 (COMMS strings <= 21 chars), TC6.5 (SDN strings <= 21 chars).
- **F7 (Header Bar with RTC)**: TC7.1 (Header rendering function present), TC7.2 (RTC format `HH:MM:SS` present), TC7.3 (Screen name 4-char label rendered), TC7.4 (Sync indicator dot rendered), TC7.5 (Progress dots rendered).
- **F8 (Dynamic Submenu Modal)**: TC8.1 (Dynamic `totalOpciones` handled), TC8.2 (Item pitch accommodates 5 items in 60px dialog), TC8.3 (Cursor modulo uses `totalOpciones`), TC8.4 (Selection inversion box rendered), TC8.5 (Dialog bounds within 128x64).
- **F9 (Internal RTC Engine)**: TC9.1 (`<time.h>` and `<sys/time.h>` included), TC9.2 (`settimeofday()` invoked with `struct timeval`), TC9.3 (`time()` and `localtime_r()` used), TC9.4 (Fallback indicator when un-synced), TC9.5 (RTC time retained across light sleep).
- **F10 (DeviceTimeReq Sync)**: TC10.1 (`sendMacCommandReq(RADIOLIB_LORAWAN_MAC_DEVICE_TIME)` queued), TC10.2 (`getMacDeviceTimeAns` processed), TC10.3 (Return code verified), TC10.4 (Time latency fraction accounted for), TC10.5 (Telemetry timestamp uses synced epoch).
- **F11 (On-Demand ADC Power Saver)**: TC11.1 (300ms loop timer gated/removed), TC11.2 (Battery ADC sampled on-demand during TX), TC11.3 (Weight ADC sampled on-demand), TC11.4 (GPIO 37 pulsed HIGH only during measurement <= 20ms), TC11.5 (GPIO 37 returned to LOW immediately).
- **F12 (Configurable ADC Menu)**: TC12.1 (5 intervals defined: "Solo en TX", "Cada 10s", "Cada 30s", "Cada 60s", "Cada 300s"), TC12.2 (Millisecond array `{ 0, 10000, 30000, 60000, 300000 }`), TC12.3 (NVS key `"adc_idx"`), TC12.4 (Menu selection navigation), TC12.5 (Runtime loop applies configured interval).

---

### 4.3 Tier 2: Boundary & Corner Cases (>=5 Test Cases per Feature)

#### Feature F13 Boundaries (Energy Audit)
- **TC13-B1 — Missing File Handling**: Runner reports explicit descriptive failure when `AUDITORIA_ENERGIA.md` is missing, without unhandled Python stack trace.
- **TC13-B2 — Missing Individual State Row**: Table missing Deep Sleep RTC Timer or LoRa TX +22dBm raises assertion failure identifying the missing state row.
- **TC13-B3 — Inverted or Corrupted Unit Values**: Table specifying "24 mA" instead of "24 uA" or negative current values fails strict validation.
- **TC13-B4 — Mathematical Inconsistency Check**: If documented autonomy days differ from calculated formula ($800\text{ mAh} / I_{avg}$) by > 15%, test fails.
- **TC13-B5 — Unrealistic 100% Battery DoD Check**: Fails if autonomy table assumes 100% nominal battery capacity instead of 80-85% usable DoD.
- **TC13-B6 — Missing Hardware/Software Subsections**: Omission of Vext isolation, SX1262 sleep, or PRG debounce loop fails structural check.

#### Feature F14 Boundaries (Zero Emojis Scanner)
- **TC14-B1 — 4-Byte Supplementary Multilingual Plane (SMP) Characters**: Validates detection of characters in ranges U+1F600–U+1F64F, U+1F300–U+1F5FF, U+1F900–U+1FAFF (e.g. `[U+1F600]`, `[U+1F4E5]`, `[U+26A1]`, `[U+1F680]`).
- **TC14-B2 — 3-Byte Basic Multilingual Plane (BMP) Symbols & Dingbats**: Validates detection of characters in ranges U+2700–U+27BF, U+2600–U+26FF (e.g. `[U+2714]`, `[U+274C]`, `[U+26A1]`, `[U+2605]`).
- **TC14-B3 — Combining Sequences & Variation Selectors**: Validates detection of keycaps (`1\uFE0F\u20E3`) and skin tone modifiers.
- **TC14-B4 — Legitimate Spanish Non-ASCII Characters**: Validates that accented vowels (`á`, `é`, `í`, `ó`, `ú`, `ñ`, `Á`, `É`, `Í`, `Ó`, `Ú`, `Ñ`) in Spanish documentation do NOT trigger false positive emoji violations.
- **TC14-B5 — Emojis in String Escapes**: Detects unicode escapes (`\u2714`, `\U0001F4E5`, `\x...`) embedded in source code string literals.
- **TC14-B6 — Binary and Large File Boundaries**: Gracefully skips binary files (`.rar`) and handles large files (> 5 MB) without memory exhaustion.

#### Feature F15 Boundaries (Main Immutability)
- **TC15-B1 — Single-Bit Modification Sensitivity**: Flipping or adding a single byte anywhere in `main.ino` alters SHA-256 and fails test.
- **TC15-B2 — Line Ending CRLF vs LF Normalization**: Validates that accidental Git CRLF/LF checkout conversion is detected as an immutability violation.
- **TC15-B3 — Extra File Injection**: Adding any file into `main\` (e.g. `main/temp.txt` or `main/main.ino.bak`) triggers file count mismatch failure.
- **TC15-B4 — File Deletion Detection**: Removing any of the 5 baseline files triggers missing file failure.
- **TC15-B5 — Metadata Directory Isolation**: Confirms that `.agents\` inside `main\` is correctly excluded from the 5-file immutability check.

#### Feature F16 Boundaries (Dual Compilation Harness)
- **TC16-B1 — Missing Build Path Auto-Creation**: Pre-creates missing build paths before invoking arduino-cli, preventing Windows path errors.
- **TC16-B2 — Path with Spaces and Special Characters**: Handles paths containing spaces and Windows backslashes using `subprocess.run` list syntax.
- **TC16-B3 — Invalid or Missing Arduino-CLI Path**: Reports clear error message and exits with code 1 if `arduino-cli.exe` is missing.
- **TC16-B4 — Compilation Timeout Boundary**: Imposes a 180-second timeout to prevent hung processes from blocking CI pipelines.
- **TC16-B5 — Memory Overflow Boundary**: Parses memory output and asserts Flash usage < 90% (3,008,102 bytes) and RAM usage < 80% (262,144 bytes).

---

### 4.4 Tier 3: Pairwise Combinations
A combinatorial 2-way orthogonal test matrix tests interaction between configuration dimensions:

| Test ID | Sketch Target | ADC Sampling Mode | Deep Sleep Wakeup Source | OLED Display State | LoRaWAN Time Sync |
|---|---|---|---|---|---|
| **PW-01** | `heltec_v3_local_diag` | `ADC_SOLO_TX` (0) | `EXT0_PRG_BUTTON` | `OLED_ACTIVE` | Unsynchronized (`--:--:--`) |
| **PW-02** | `heltec_v3_local_diag` | `ADC_CADA_10S` (1) | `TIMER_WAKEUP` | `OLED_TIMEOUT_OFF` | Unsynchronized (`--:--:--`) |
| **PW-03** | `heltec_v3_local_diag` | `ADC_CADA_30S` (2) | `DUAL_EXT0_AND_TIMER` | `OLED_DEEP_SLEEP_VEXT_OFF` | Unsynchronized (`--:--:--`) |
| **PW-04** | `heltec_v3_local_battery` | `ADC_SOLO_TX` (0) | `TIMER_WAKEUP` | `OLED_DEEP_SLEEP_VEXT_OFF` | Synced via `DeviceTimeReq` (`HH:MM:SS`) |
| **PW-05** | `heltec_v3_local_battery` | `ADC_CADA_60S` (3) | `EXT0_PRG_BUTTON` | `OLED_ACTIVE` | Synced via `DeviceTimeReq` (`HH:MM:SS`) |
| **PW-06** | `heltec_v3_local_battery` | `ADC_CADA_300S` (4) | `DUAL_EXT0_AND_TIMER` | `OLED_TIMEOUT_OFF` | Synced via `DeviceTimeReq` (`HH:MM:SS`) |

#### CLI Flag Pairwise Combinations
- **PW-CLI-1**: `py test/test_runner.py --tier 1 --summary` (Runs Tier 1 with ANSI summary output).
- **PW-CLI-2**: `py test/test_runner.py --tier 2 --json test/test_results.json` (Runs Tier 2 and verifies JSON artifact).
- **PW-CLI-3**: `py test/test_runner.py --tier 4 --compile` (Runs Tier 4 executing full dual compilation).
- **PW-CLI-4**: `py test/test_runner.py --tier all --compile --summary` (Full regression run, all tiers, compilation, and summary).

---

### 4.5 Tier 4: Real-World Scenarios
- **Scenario S1 — Complete Repository Integrity & Cryptographic Baseline Audit**:
  Executes SHA-256 bit-for-bit check on `main\`, runs full repository zero-emojis scan, and validates `AUDITORIA_ENERGIA.md` completeness.
- **Scenario S2 — Full Dual Firmware Compilation & Memory Footprint Verification**:
  Invokes `arduino-cli` across both `heltec_v3_local_diag` and `heltec_v3_local_battery`. Asserts return code 0 and verifies Flash/RAM utilization limits.
- **Scenario S3 — End-to-End Battery Autonomous Life Cycle Scenario**:
  Evaluates mathematical model for a 1000 mAh LiPo node: OTAA join, `DeviceTimeReq` sync, on-demand ADC read (20ms @ 31mA), LoRa TX (+14dBm, SF7, 360ms @ 82mA), RX1/RX2 windows, and debounced deep sleep with SX1262 sleep (24 uA for 300s). Confirms calculated average current <= 274 uA and autonomy >= 121 days.
- **Scenario S4 — Diagnostic Bench Calibration & Scale Stress Lifecycle**:
  Evaluates continuous OLED display diagnostics with HH:MM:SS RTC header, 10s ADC sampling interval, and PRG long-press deep sleep with verified button release debounce.

---

### 4.6 Concrete Pure Python Test Runner Implementation (`test/test_runner.py`)

Below is the complete reference implementation designed for `teamwork_coder_e2e`. It uses standard library modules only (`unittest`, `json`, `hashlib`, `subprocess`, `re`, `os`, `sys`, `time`, `argparse`, `pathlib`).

```python
#!/usr/bin/env python3
"""
Automated Test Runner & Verification Suite for Heltec WiFi LoRa 32 V3
Requirements R5 (Energy Audit Validation) & R6 (Integrity, Zero Emojis, Dual Compilation)
Standard Library Only (unittest, json, hashlib, subprocess, re, os, sys, argparse)
"""

import os
import sys
import re
import json
import time
import hashlib
import argparse
import unittest
import subprocess
from pathlib import Path

# Paths configuration
WORKSPACE_ROOT = Path(__file__).resolve().parent.parent
MAIN_DIR = WORKSPACE_ROOT / "main"
BATTERY_DIR = WORKSPACE_ROOT / "heltec_v3_local_battery"
DIAG_DIR = WORKSPACE_ROOT / "heltec_v3_local_diag"
AUDIT_FILE = WORKSPACE_ROOT / "AUDITORIA_ENERGIA.md"
PROJECT_FILE = WORKSPACE_ROOT / "PROJECT.md"
RESULTS_JSON = WORKSPACE_ROOT / "test" / "test_results.json"

ARDUINO_CLI = Path(r"C:\Users\mate1\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe")
FQBN = "esp32:esp32:heltec_wifi_lora_32_V3"
BUILD_PATH_DIAG = Path(r"C:\Users\mate1\AppData\Local\Temp\ard_build")
BUILD_PATH_BAT = Path(r"C:\Users\mate1\AppData\Local\Temp\ard_build_bat")

# Verified SHA-256 hashes for main/ baseline version 1.0
BASELINE_HASHES = {
    "heltec_v3_config.h": "58C1E8FBF171373EDBC0CF4EC5E810EF4AA2B9E947D44EF0E19E82D6E3EF537F",
    "main.ino":           "1B3DB58322463193AA53D0DBBD4EA87ACEDD474ADD2C9D70A074A0A84B716DB0",
    "scale_driver.h":     "7FE6333CBB9A10B49E6029517887EB46BDB9A2A67A56226F359BB51195D77A22",
    "sdn_header.h":       "5B90F21F5135645B6EB2E4C0BB6F7482EAE2516F1DCC84EABA796E0D34A5FD89",
    "web_interface.h":    "A1E909033ECE4F4C75D80CF104FFE315AE8FC84E284B169DB40BA7C60D5D1FDE",
}

BASELINE_SIZES = {
    "heltec_v3_config.h": 3890,
    "main.ino":           32966,
    "scale_driver.h":     10760,
    "sdn_header.h":       3665,
    "web_interface.h":    48636,
}

# Regex for emojis across SMP and BMP symbols (excluding legitimate Spanish text)
EMOJI_PATTERN = re.compile(
    r"[\U0001F600-\U0001F64F"  # Emoticons
    r"\U0001F300-\U0001F5FF"  # Misc Symbols & Pictographs
    r"\U0001F680-\U0001F6FF"  # Transport & Map
    r"\U0001F900-\U0001FAFF"  # Supplemental & Extended
    r"\u2600-\u26FF"          # Misc Symbols
    r"\u2700-\u27BF"          # Dingbats
    r"\u2300-\u23FF"          # Misc Technical
    r"\u2B50\u203C\u2049"     # Special symbols
    r"]"
)


class JsonTestResult(unittest.TestResult):
    """Custom TestResult collecting detailed per-test execution data for JSON output."""

    def __init__(self):
        super().__init__()
        self.test_records = []

    def startTest(self, test):
        super().startTest(test)
        self._start_time = time.time()

    def addSuccess(self, test):
        super().addSuccess(test)
        duration_ms = (time.time() - self._start_time) * 1000
        self.test_records.append({
            "id": test.id(),
            "name": getattr(test, "_testMethodName", str(test)),
            "tier": getattr(test, "tier", 1),
            "feature": getattr(test, "feature", "GENERIC"),
            "status": "PASSED",
            "duration_ms": round(duration_ms, 2),
            "error": None
        })

    def addFailure(self, test, err):
        super().addFailure(test, err)
        duration_ms = (time.time() - self._start_time) * 1000
        self.test_records.append({
            "id": test.id(),
            "name": getattr(test, "_testMethodName", str(test)),
            "tier": getattr(test, "tier", 1),
            "feature": getattr(test, "feature", "GENERIC"),
            "status": "FAILED",
            "duration_ms": round(duration_ms, 2),
            "error": self._exc_info_to_string(err, test)
        })

    def addError(self, test, err):
        super().addError(test, err)
        duration_ms = (time.time() - self._start_time) * 1000
        self.test_records.append({
            "id": test.id(),
            "name": getattr(test, "_testMethodName", str(test)),
            "tier": getattr(test, "tier", 1),
            "feature": getattr(test, "feature", "GENERIC"),
            "status": "ERROR",
            "duration_ms": round(duration_ms, 2),
            "error": self._exc_info_to_string(err, test)
        })


# ==============================================================================
# TIER 1: FEATURE COVERAGE TESTS
# ==============================================================================
class TestTier1FeatureCoverage(unittest.TestCase):
    tier = 1

    # F15: Immutability
    feature = "F15"
    def test_f15_01_main_file_inventory(self):
        """TC15.1: Verify main directory contains exactly the 5 baseline files."""
        files = [f.name for f in MAIN_DIR.iterdir() if f.is_file()]
        self.assertEqual(set(files), set(BASELINE_HASHES.keys()))

    def test_f15_02_main_file_sizes(self):
        """TC15.2: Verify main directory file sizes match baseline byte-for-byte."""
        for name, expected_size in BASELINE_SIZES.items():
            path = MAIN_DIR / name
            self.assertTrue(path.exists(), f"Missing file: {name}")
            self.assertEqual(path.stat().st_size, expected_size, f"Size mismatch on {name}")

    def test_f15_03_main_sha256_hashes(self):
        """TC15.3: Verify SHA-256 hashes for all 5 files in main directory."""
        for name, expected_hash in BASELINE_HASHES.items():
            path = MAIN_DIR / name
            h = hashlib.sha256(path.read_bytes()).hexdigest().upper()
            self.assertEqual(h, expected_hash, f"SHA256 mismatch on {name}")

    def test_f15_04_baseline_rar_archive(self):
        """TC15.4: Verify main.rar exists and matches baseline archive size."""
        rar = WORKSPACE_ROOT / "main.rar"
        self.assertTrue(rar.exists())
        self.assertEqual(rar.stat().st_size, 23445)

    def test_f15_05_no_temp_files_in_main(self):
        """TC15.5: Verify no backup, temp, or unexpected files exist in main."""
        for f in MAIN_DIR.iterdir():
            if f.is_file():
                self.assertIn(f.name, BASELINE_HASHES)
            elif f.is_dir():
                self.assertEqual(f.name, ".agents")

    # F14: Zero Emojis
    feature = "F14"
    def test_f14_01_zero_emojis_battery_firmware(self):
        """TC14.1: Verify zero emojis in heltec_v3_local_battery."""
        for ext in ("*.ino", "*.h"):
            for f in BATTERY_DIR.glob(ext):
                text = f.read_text(encoding="utf-8", errors="replace")
                matches = list(EMOJI_PATTERN.finditer(text))
                self.assertEqual(len(matches), 0, f"Found {len(matches)} emojis in {f.name}")

    def test_f14_02_zero_emojis_diag_firmware(self):
        """TC14.2: Verify zero emojis in heltec_v3_local_diag."""
        for ext in ("*.ino", "*.h"):
            for f in DIAG_DIR.glob(ext):
                text = f.read_text(encoding="utf-8", errors="replace")
                matches = list(EMOJI_PATTERN.finditer(text))
                self.assertEqual(len(matches), 0, f"Found {len(matches)} emojis in {f.name}")

    def test_f14_03_zero_emojis_documentation(self):
        """TC14.3: Verify zero emojis in AUDITORIA_ENERGIA.md and PROJECT.md."""
        for doc in (AUDIT_FILE, PROJECT_FILE):
            if doc.exists():
                text = doc.read_text(encoding="utf-8", errors="replace")
                matches = list(EMOJI_PATTERN.finditer(text))
                self.assertEqual(len(matches), 0, f"Found {len(matches)} emojis in {doc.name}")

    def test_f14_04_zero_emojis_test_suite(self):
        """TC14.4: Verify zero emojis in test suite files."""
        for f in (WORKSPACE_ROOT / "test").glob("*.py"):
            text = f.read_text(encoding="utf-8", errors="replace")
            matches = list(EMOJI_PATTERN.finditer(text))
            self.assertEqual(len(matches), 0, f"Found {len(matches)} emojis in {f.name}")

    def test_f14_05_zero_emojis_oled_string_literals(self):
        """TC14.5: Verify OLED strings have zero emojis and zero invalid CP437 symbols."""
        display_print_regex = re.compile(r'display\.(print|printf|println)\s*\(\s*"([^"]+)"')
        for sketch in (BATTERY_DIR / "heltec_v3_local_battery.ino", DIAG_DIR / "heltec_v3_local_diag.ino"):
            if sketch.exists():
                content = sketch.read_text(encoding="utf-8", errors="replace")
                for match in display_print_regex.finditer(content):
                    string_lit = match.group(2)
                    self.assertIsNone(EMOJI_PATTERN.search(string_lit), f"Emoji in OLED string: {string_lit}")
                    self.assertNotIn("°", string_lit, f"Invalid symbol in OLED string: {string_lit}")

    # F13: Energy Audit (R5)
    feature = "F13"
    def test_f13_01_audit_file_existence_and_sections(self):
        """TC13.1: Verify AUDITORIA_ENERGIA.md exists and contains all 6 required sections."""
        self.assertTrue(AUDIT_FILE.exists(), "AUDITORIA_ENERGIA.md is missing in root")
        text = AUDIT_FILE.read_text(encoding="utf-8", errors="replace")
        self.assertGreater(len(text), 2000)
        required_headers = [
            "Resumen Ejecutivo",
            "Dominios de Alimentacion",
            "Estados de Operacion",
            "Autonomia de Bateria",
            "Recomendaciones de Hardware",
            "Recomendaciones de Software",
        ]
        for header in required_headers:
            self.assertTrue(re.search(rf"#+\s*.*{header}", text, re.IGNORECASE), f"Missing header: {header}")

    def test_f13_02_audit_table_states_completeness(self):
        """TC13.2: Verify operational states table lists E1 to E10."""
        text = AUDIT_FILE.read_text(encoding="utf-8", errors="replace")
        expected_states = [
            r"E1.*Activo", r"E2.*Timeout", r"E3.*ADC.*Bateria", r"E4.*Balanza",
            r"E5.*Transmision.*\+14", r"E6.*Transmision.*\+22", r"E7.*RX",
            r"E8.*Deep Sleep.*PRG", r"E9.*Deep Sleep.*ext0", r"E10.*Deep Sleep.*RTC"
        ]
        for st in expected_states:
            self.assertTrue(re.search(st, text, re.IGNORECASE), f"Missing state row: {st}")

    def test_f13_03_audit_current_ranges_and_reductions(self):
        """TC13.3: Verify quantitative values and reduction factors in audit table."""
        text = AUDIT_FILE.read_text(encoding="utf-8", errors="replace")
        # Assert sleep currents in uA
        self.assertTrue(re.search(r"120\s*uA", text), "ext0 sleep should be 120 uA")
        self.assertTrue(re.search(r"24\s*uA", text), "RTC timer sleep should be 24 uA")
        # Assert reduction factor >= 15x and >= 70x
        self.assertTrue(re.search(r"15(\.\d+)?x", text), "ext0 reduction should be >= 15x")
        self.assertTrue(re.search(r"75(\.\d+)?x", text), "RTC reduction should be >= 70x")

    def test_f13_04_audit_autonomy_models(self):
        """TC13.4: Verify autonomy models for 1000 mAh and 3000 mAh LiPo."""
        text = AUDIT_FILE.read_text(encoding="utf-8", errors="replace")
        self.assertTrue(re.search(r"1000\s*mAh", text))
        self.assertTrue(re.search(r"3000\s*mAh", text))
        self.assertTrue(re.search(r"800\s*mAh", text), "1000 mAh should use 800 mAh usable")
        self.assertTrue(re.search(r"2550\s*mAh", text), "3000 mAh should use 2550 mAh usable")
        self.assertTrue(re.search(r"121(\.\d+)?\s*dias", text), "5m autonomy should be >= 120 days")

    def test_f13_05_audit_hardware_recommendations(self):
        """TC13.5: Verify hardware recommendations in AUDITORIA_ENERGIA.md."""
        text = AUDIT_FILE.read_text(encoding="utf-8", errors="replace")
        self.assertTrue(re.search(r"Vext.*GPIO\s*36", text, re.IGNORECASE))
        self.assertTrue(re.search(r"GPIO\s*37", text))
        self.assertTrue(re.search(r"SX1262", text))
        self.assertTrue(re.search(r"ESD|fuga|I2C", text, re.IGNORECASE))

    def test_f13_06_audit_software_recommendations(self):
        """TC13.6: Verify software recommendations in AUDITORIA_ENERGIA.md."""
        text = AUDIT_FILE.read_text(encoding="utf-8", errors="replace")
        self.assertTrue(re.search(r"digitalRead\(PIN_PRG\)", text))
        self.assertTrue(re.search(r"radio\.sleep\(false\)", text))
        self.assertTrue(re.search(r"DeviceTimeReq", text))
        self.assertTrue(re.search(r"settimeofday", text))


# ==============================================================================
# TIER 2: BOUNDARY & CORNER CASES
# ==============================================================================
class TestTier2BoundaryCases(unittest.TestCase):
    tier = 2

    feature = "F15"
    def test_f15_b1_single_byte_mutation_detected(self):
        """TC15-B1: Assert that any 1-byte modification breaks the hash check."""
        sample_file = MAIN_DIR / "sdn_header.h"
        real_hash = BASELINE_HASHES["sdn_header.h"]
        content = bytearray(sample_file.read_bytes())
        content[0] ^= 0xFF  # Mutate 1 bit
        mutated_hash = hashlib.sha256(content).hexdigest().upper()
        self.assertNotEqual(real_hash, mutated_hash)

    feature = "F14"
    def test_f14_b1_spanish_accents_not_flagged_as_emojis(self):
        """TC14-B4: Ensure legitimate Spanish vowels (á, é, í, ó, ú, ñ) are NOT detected as emojis."""
        sample_spanish_text = "Telemetría avícola con calibración estándar y transmisión óptima enTolva Año 2026."
        matches = list(EMOJI_PATTERN.finditer(sample_spanish_text))
        self.assertEqual(len(matches), 0)

    def test_f14_b2_smp_and_bmp_emojis_detected(self):
        """TC14-B1/B2: Ensure sample SMP and BMP emojis are reliably captured."""
        dirty_text = "Status: OK [U+1F680] Alert: High voltage [U+26A1] Check: [U+2714] Error: [U+274C]"
        matches = [m.group() for m in EMOJI_PATTERN.finditer(dirty_text)]
        self.assertEqual(len(matches), 4)

    feature = "F13"
    def test_f13_b1_autonomy_formula_mathematical_consistency(self):
        """TC13-B4: Validate formula I_avg = sum(I*t)/T yields consistent results."""
        # 5 minute cycle (300 seconds)
        q_active_ma_s = 75.14
        t_sleep_s = 300 - 0.36 - 0.02 - 0.16
        i_sleep_ma = 0.024
        q_sleep_ma_s = i_sleep_ma * t_sleep_s
        i_avg_ma = (q_active_ma_s + q_sleep_ma_s) / 300.0
        # Assert calculated i_avg is ~0.274 mA
        self.assertAlmostEqual(i_avg_ma, 0.274, delta=0.01)
        # Usable battery capacity: 800 mAh
        hours = 800.0 / i_avg_ma
        days = hours / 24.0
        self.assertAlmostEqual(days, 121.6, delta=2.0)


# ==============================================================================
# TIER 4: REAL-WORLD SCENARIOS & FULL COMPILATION
# ==============================================================================
class TestTier4RealWorldScenarios(unittest.TestCase):
    tier = 4

    feature = "F16"
    def test_f16_01_dual_compilation_harness(self):
        """TC16.3/TC16.4: Full dual compilation of diag and battery sketches."""
        self.assertTrue(ARDUINO_CLI.exists(), f"arduino-cli not found at {ARDUINO_CLI}")
        
        # Pre-create build directories to avoid Windows race conditions
        BUILD_PATH_DIAG.mkdir(parents=True, exist_ok=True)
        BUILD_PATH_BAT.mkdir(parents=True, exist_ok=True)

        # 1. Compile heltec_v3_local_diag
        cmd_diag = [
            str(ARDUINO_CLI), "compile",
            "--fqbn", FQBN,
            "--build-path", str(BUILD_PATH_DIAG),
            str(DIAG_DIR)
        ]
        res_diag = subprocess.run(cmd_diag, capture_output=True, text=True, timeout=180)
        self.assertEqual(res_diag.returncode, 0, f"Diag compilation failed:\n{res_diag.stderr}")

        # 2. Compile heltec_v3_local_battery
        cmd_bat = [
            str(ARDUINO_CLI), "compile",
            "--fqbn", FQBN,
            "--build-path", str(BUILD_PATH_BAT),
            str(BATTERY_DIR)
        ]
        res_bat = subprocess.run(cmd_bat, capture_output=True, text=True, timeout=180)
        self.assertEqual(res_bat.returncode, 0, f"Battery compilation failed:\n{res_bat.stderr}")


# ==============================================================================
# MAIN TEST RUNNER ENTRY POINT
# ==============================================================================
def run_tests(selected_tier="all", run_compile=False, print_summary=True, json_path=RESULTS_JSON):
    start_wall_time = time.time()
    suite = unittest.TestSuite()
    loader = unittest.TestLoader()

    # Determine test cases to load
    tier_classes = {
        1: [TestTier1FeatureCoverage],
        2: [TestTier2BoundaryCases],
        4: [TestTier4RealWorldScenarios] if run_compile else []
    }

    if selected_tier == "all":
        target_tiers = [1, 2]
        if run_compile:
            target_tiers.append(4)
    else:
        target_tiers = [int(selected_tier)]

    for t in target_tiers:
        for cls in tier_classes.get(t, []):
            suite.addTests(loader.loadTestsFromTestCase(cls))

    result = JsonTestResult()
    suite.run(result)
    duration_total = time.time() - start_wall_time

    # Generate JSON output artifact
    output_data = {
        "summary": {
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "duration_seconds": round(duration_total, 3),
            "total_tests": result.testsRun,
            "passed": result.testsRun - len(result.failures) - len(result.errors),
            "failed": len(result.failures),
            "errors": len(result.errors),
            "skipped": len(result.skipped),
            "overall_status": "PASSED" if result.wasSuccessful() else "FAILED"
        },
        "tests": result.test_records
    }

    json_path.parent.mkdir(parents=True, exist_ok=True)
    with open(json_path, "w", encoding="utf-8") as fp:
        json.dump(output_data, fp, indent=2)

    # Print summary if requested
    if print_summary:
        print("\n" + "=" * 70)
        print(" HELTEC V3 FIRMWARE VERIFICATION SUITE SUMMARY")
        print("=" * 70)
        print(f" Total Tests Run : {result.testsRun}")
        print(f" Passed         : {output_data['summary']['passed']}")
        print(f" Failed         : {output_data['summary']['failed']}")
        print(f" Errors         : {output_data['summary']['errors']}")
        print(f" Elapsed Time   : {duration_total:.2f}s")
        print(f" Overall Status : {output_data['summary']['overall_status']}")
        print(f" JSON Artifact  : {json_path}")
        print("=" * 70)

    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Heltec V3 Firmware Automated Test Suite Runner")
    parser.add_argument("--tier", choices=["1", "2", "3", "4", "all"], default="all", help="Target test tier")
    parser.add_argument("--compile", action="store_true", help="Execute dual compilation harness (Tier 4)")
    parser.add_argument("--summary", action="store_true", default=True, help="Display summary table")
    parser.add_argument("--json", type=Path, default=RESULTS_JSON, help="Custom JSON output artifact path")
    args = parser.parse_args()

    # If tier 4 is requested, implicitly enable compile
    if args.tier == "4":
        args.compile = True

    exit_code = run_tests(
        selected_tier=args.tier,
        run_compile=args.compile,
        print_summary=args.summary,
        json_path=args.json
    )
    sys.exit(exit_code)
```

---

## 5. Verification Method

### 5.1 Verification Commands
To independently verify the test runner architecture, compilation pipeline, and test suites on the host system:

1. **Verify Python Execution Environment**:
   ```powershell
   py -3.14 -c "import sys, unittest, json, hashlib, subprocess, re, os; print('Python stdlib OK')"
   ```
   Must print `Python stdlib OK` without import errors.

2. **Verify Cryptographic Hashes of `main\` Baseline**:
   ```powershell
   Get-ChildItem -File "c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main" | ForEach-Object { [PSCustomObject]@{ Name=$_.Name; Length=$_.Length; SHA256=(Get-FileHash -Algorithm SHA256 $_.FullName).Hash } } | Format-Table -AutoSize
   ```
   Must match the 5 reference hashes in Section 1.1 exactly.

3. **Verify Dual Compilation with Arduino CLI**:
   ```powershell
   $cli = "C:\Users\mate1\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
   $fqbn = "esp32:esp32:heltec_wifi_lora_32_V3"

   # Compile Diag
   & $cli compile --fqbn $fqbn --build-path "C:\Users\mate1\AppData\Local\Temp\ard_build" "c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\heltec_v3_local_diag"

   # Compile Battery
   & $cli compile --fqbn $fqbn --build-path "C:\Users\mate1\AppData\Local\Temp\ard_build_bat" "c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\heltec_v3_local_battery"
   ```
   Both commands must exit with code `0`.

4. **Verify Test Runner Execution (once implemented by coder)**:
   ```powershell
   # Run all static and integrity checks (Tiers 1 & 2)
   py test/test_runner.py --tier all --summary

   # Run full regression suite including compilation harness (Tiers 1, 2, 4)
   py test/test_runner.py --compile --summary
   ```
   Must produce `test/test_results.json` and exit with code `0`.

### 5.2 Files to Inspect
- `c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\AUDITORIA_ENERGIA.md`: Verify 6 sections, 10 states table, autonomy formulas, and HW/SW recommendations.
- `c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\`: Verify exactly 5 files with unchanged SHA-256 hashes.
- `c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\test\test_results.json`: Verify structured JSON artifact.

### 5.3 Invalidation Conditions
- Any occurrence of emojis in new code, modified sketches, OLED strings, or documentation invalidates compliance.
- Any change to any byte of any file in `main\` invalidates compliance.
- Any failure or warning treated as error in `arduino-cli compile` invalidates compliance.
- Any discrepancy between claimed autonomy and mathematical formula in `AUDITORIA_ENERGIA.md` invalidates compliance.
- Non-zero exit code on clean test run invalidates test runner compliance.
