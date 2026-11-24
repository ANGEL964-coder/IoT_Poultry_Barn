# Empirical Challenger Verification & Adversarial Audit Report (Challenger 2)

**Track**: E2E Verification & Integrity Track  
**Role**: Empirical Challenger (`teamwork_challenger`)  
**Date**: 2026-11-15T23:31:00Z  
**Verdict**: **APPROVE**  

---

## 1. Observation

### Obs 1.1: Button Debounce Logic Model (79 ms Bounce vs >= 80 ms Continuous HIGH)
- In `heltec_v3_local_battery/heltec_v3_local_battery.ino` (lines 256-271):
  ```cpp
  pinMode(PIN_PRG, INPUT_PULLUP);
  uint32_t t_inicio_high = 0;
  while (true) {
      if (digitalRead(PIN_PRG) == LOW) {
          t_inicio_high = 0; // Rebote o pulsacion detectada, reiniciar contador
          delay(5);
      } else {
          if (t_inicio_high == 0) {
              t_inicio_high = millis();
          } else if (millis() - t_inicio_high >= PRG_DEBOUNCE_RELEASE_MS) {
              break; // Estable en HIGH durante >= 80 ms continuos
          }
          delay(5);
      }
  }
  ```
- In `heltec_v3_local_battery/heltec_v3_config.h` (line 43):
  `#define PRG_DEBOUNCE_RELEASE_MS 80`
- Executed `empirical_challenger_harness.py` test `test_debounce_model()`:
  - Trace with 79 ms HIGH followed by 10 ms LOW bounce: `passed_79 == False` (sleep not triggered, counter reset).
  - Trace with 80 ms continuous HIGH: `passed_80 == True` (sleep triggered at index 79, exactly 80 ms).
  - 10,000 randomized traces with bounce noise: 3,699 accepted (where contiguous HIGH was >= 80 ms), 6,301 rejected (< 80 ms). Zero false triggers.

### Obs 1.2: OLED Display Geometry and Character Boundaries
- SSD1306 resolution: 128 px width x 64 px height. Adafruit_GFX font size 1 glyph pitch: 6 px horizontal (5 px glyph + 1 px spacing), 8 px vertical.
- Calculation for 22 characters:
  `22 * 6 = 132 px > 128 px` (overflows by exactly 4 px; glyph at index 21 occupies x=126 to 131, clipping 4 px outside the 128 px framebuffer).
- Calculation for 21 characters:
  `21 * 6 = 126 px <= 128 px` (occupies x=0 to 125, leaving 2 px of right margin inside the 128 px boundary).
- 5-item submenu modal layout in `heltec_v3_local_battery.ino` (lines 747-777):
  - Modal outer rectangle: `display.drawRect(2, 2, 124, 60, SSD1306_WHITE)` covers y=2 to y=61.
  - Options pitch layout: `y_start = 15`, `y_step = 9`.
  - Item y-coordinates: `[15, 24, 33, 42, 51]`.
  - 5th item at y=51: Text bottom edge = `51 + 8 = 59 px <= 64 px`.
  - Selection highlight box: `fillRect(5, 51-1, 118, 9, WHITE)` spans y=50 to 58.
  - Clearance between selection box bottom (y=58) and dialog box bottom border (y=61) is 2 px; clearance to screen display edge (y=63) is 5 px.

### Obs 1.3: POSIX Internal RTC Engine & LoRaWAN DeviceTimeAns Latency Math
- Upstream RadioLib library source code at `C:\Users\mate1\Documents\Arduino\libraries\RadioLib\src\protocols\LoRaWAN\LoRaWAN.cpp` (lines 3004-3018):
  ```cpp
  // calculate the millisecond fraction
  RadioLibTime_t ms = (RadioLibTime_t)payload[4] * 1000UL / 256UL;

  // add offset between current time and end of uplink transmission
  ms += mod->hal->millis() - this->tUplinkEnd;

  if(timestamp) { 
    *timestamp = LoRaWANNode::ntoh<uint32_t>(&payload[0]);
    *timestamp += ms / 1000;
    if(returnUnix) {
      uint32_t unixOffset = 315964800UL - 18UL; // 18 leap seconds since GPS epoch (Jan. 6th 1980)
      *timestamp += unixOffset;
    }
  }
  if(fraction) { *fraction = ms % 1000; }
  ```
- Conversion constants verified:
  - GPS Epoch: 1980-01-06 00:00:00 UTC. Unix Epoch: 1970-01-01 00:00:00 UTC.
  - Days between epochs: 3,657 days. Nominal seconds = 315,964,800 s.
  - Leap seconds accumulated through 2026: 18 s.
  - Net offset: `315964800 - 18 = 315,964,782 seconds`.
- ESP32-S3 `time_t` is signed 64-bit (`int64_t`), tested past the 32-bit limit `0x7FFFFFFF` (2,147,483,647 / 2038-01-19) with Year 2050 epoch (2,524,608,000 s).

### Obs 1.4: ADC Battery Divider Math and Float Limits
- In `heltec_v3_config.h` (lines 62-63):
  `#define DIVIDER_FACTOR 4.90f`
  `#define FACTOR_CALIBRACION 1.00f`
- Voltage calculation: `v = ((suma_mv / 16.0f) / 1000.0f) * DIVIDER_FACTOR * FACTOR_CALIBRACION`.
  - Nominal VBAT 4.20 V / 4.90 = 0.85714 V (857 mV), within ESP32-S3 ADC linear calibration range.
  - 16-sample sum maximum uint32 value is 52,800 << 4,294,967,295 (zero risk of integer overflow).
- Clamp & Fallback logic in `heltec_v3_local_battery.ino` (lines 171-174):
  `if (v < 2.5f || v > 4.4f) return 4.12f;`
- Piecewise LiPo percentage table (`voltajes[] = {4.20, 4.10, ..., 3.40}`) is strictly monotonic decreasing with non-zero interval denominators, guaranteeing bounds in `[0, 100]%` and zero divide-by-zero risk.
- Scale zero-slope guard: `if (cal_slope_v_per_kg > 0.0001f)` cleanly prevents divide-by-zero or negative slopes.

### Obs 1.5: Automated Test Runner Execution
- Executed command: `py test/test_runner.py --tier all`
- Verbatim execution output:
  ```
  ============================================================================
   HELTEC V3 FIRMWARE VERIFICATION SUITE - EXECUTION SUMMARY
  ============================================================================
   Timestamp        : 2026-11-15T23:30:04Z
   Total Duration   : 0.14 seconds
   Overall Status   : [PASSED]
  ----------------------------------------------------------------------------
   TIER BREAKDOWN:
     Tier                         | Total  | Passed | Failed | Skipped
     -----------------------------+--------+--------+--------+-------
     Tier 1: Feature Coverage     | 81     | 81     | 0      | 0     
     Tier 2: Boundary Cases       | 40     | 40     | 0      | 0     
     Tier 3: Pairwise Combinations | 16     | 16     | 0      | 0     
     Tier 4: Real-World Scenarios | 8      | 7      | 0      | 1     
  ----------------------------------------------------------------------------
   TOTAL TESTS RUN  : 145
   PASSED           : 144
   FAILED           : 0
   ERRORS           : 0
   SKIPPED          : 1
  ============================================================================

   JSON Artifact: C:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\test\test_results.json
  ```

---

## 2. Logic Chain

1. **Debounce Logic Reliability**:
   - From Obs 1.1, the loop condition checks whether `digitalRead(PIN_PRG) == LOW`. Whenever a bounce contact occurs, `t_inicio_high` is unconditionally set to 0.
   - For any pulse duration `< 80 ms` (such as 79 ms), the counter resets before reaching `PRG_DEBOUNCE_RELEASE_MS`. Therefore, sleep is not triggered prematurely while the button is vibrating or being held.
   - Once continuous HIGH is sustained for `>= 80 ms`, `millis() - t_inicio_high >= 80` evaluates to true, breaking the loop and proceeding to deep sleep.
   - The 10,000-iteration randomized stress test confirms the algorithm is immune to chatter false-positives.

2. **OLED Layout Correctness**:
   - From Obs 1.2, 22 characters at 6 px/character equals 132 px, exceeding the 128 px hardware boundary by 4 px.
   - Restricting strings to `<= 21 characters` ensures text width is `<= 126 px`, leaving a safe 2 px right margin.
   - In `heltec_v3_local_battery.ino`, all clumsy footer legends have been completely removed, and all display strings satisfy the 21-character limit.
   - The 5-item vertical submenu with 9 px pitch spans y=15 to y=51, with the 5th item bounding box terminating at y=58/59, well within the 64 px display canvas and within the 60 px modal box.

3. **Timekeeping Protocol Compliance**:
   - From Obs 1.3, the conversion between GPS epoch and Unix epoch mathematically requires accounting for the 18 leap seconds introduced since 1980.
   - Upstream RadioLib directly implements `315964800UL - 18UL = 315964782UL`.
   - Transit delay compensation adds `millis() - tUplinkEnd` to the fraction, rolling over seconds into `epoch_unix` via integer division.
   - The ESP32-S3 toolchain utilizes 64-bit signed `time_t`, eliminating the Year 2038 overflow.

4. **Analog Conditioning Math**:
   - From Obs 1.4, the 4.9:1 resistor divider and 16-sample averaging deliver linear voltage representation.
   - Clamping to `[2.5V, 4.4V]` prevents aberrant percentage calculations when operating on USB without a LiPo battery.
   - Both battery and load-cell algorithms include strict non-zero divisor guards.

5. **Suite Completeness & Integrity**:
   - From Obs 1.5, executing the automated test suite across all four tiers confirms that all 144 executed assertions pass without failures or errors.
   - The SHA-256 baseline hashes for `main/` remain bit-for-bit identical, preserving Version 1.0 immutability.
   - Zero emojis were detected across the entire repository.

---

## 3. Caveats

1. **Compilation Flag in CI/Local Harness**:
   - In Tier 4, `test_s04_dual_firmware_compilation` is skipped when `test_runner.py` is invoked without `--compile`. When `--compile` is passed, `arduino-cli` compiles both sketches successfully, but running multiple `--compile` commands simultaneously across parallel processes can cause temporary file-lock contention on `C:\Users\mate1\AppData\Local\Temp\ard_build`.
2. **Sub-5ms Glitch Filtering**:
   - In `entrarEnDeepSleepSeguro`, polling occurs every 5 ms (`delay(5)`). A microsecond glitch occurring entirely between two 5 ms sample points would not be caught by discrete polling; however, mechanical switch bounce on tactile buttons spans milliseconds or is filtered by hardware RC circuits, and the 80 ms continuous threshold ensures total stability before sleep.
3. **Diagnostic Sketch Milestone Scope**:
   - The diagnostic sketch `heltec_v3_local_diag.ino` still contains legacy footer strings from v1.0, which is expected per `PROJECT.md` where Milestone M2 is scheduled after M1. Milestone M1 (`heltec_v3_local_battery.ino`) is fully compliant.

---

## 4. Conclusion

The test suite, mathematical models, electrical timing boundaries, OLED display geometry, internal RTC synchronization, and battery divider calculations have been adversarially challenged and empirically verified. All assertions reflect genuine physical, mathematical, protocol, and hardware invariants.

**Verdict**: **APPROVE**

---

## 5. Verification Method

To independently verify these results:

1. **Run Full Test Suite (Tiers 1 through 4)**:
   ```powershell
   cd c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot
   py test/test_runner.py --tier all
   ```
   *Expected outcome*: 145 total tests, 144 passed, 0 failed, 1 skipped (compilation), exit code 0.

2. **Run Empirical Challenger Stress Harness**:
   ```powershell
   py c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\challenger_e2e_2\empirical_challenger_harness.py
   ```
   *Expected outcome*: All 4 challenge domains pass, including 10,000 bounce iterations.

3. **Verify Upstream RadioLib Leap Second & Offset Math**:
   Inspect line 3014 of `C:\Users\mate1\Documents\Arduino\libraries\RadioLib\src\protocols\LoRaWAN\LoRaWAN.cpp` to confirm `315964800UL - 18UL`.

4. **Verify Main Immutability**:
   ```powershell
   py -c "from test.harness_immutability import verify_main_immutability; print(verify_main_immutability())"
   ```
   *Expected outcome*: `(True, [], ...)`

5. **Invalidation Conditions**:
   - Any test failure in `test/test_results.json`.
   - Changing `PRG_DEBOUNCE_RELEASE_MS` to `< 80` or removing the `t_inicio_high = 0` bounce reset.
   - Any OLED display string exceeding 21 characters in font size 1.
   - Any modification to files in `main/`.
