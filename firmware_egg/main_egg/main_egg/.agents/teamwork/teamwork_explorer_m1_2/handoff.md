# Handoff Report: OLED UI Redesign and Modal Overhaul (Features F5, F6, F7, F8, F14)

**Module**: `heltec_v3_local_battery/` (Heltec WiFi LoRa 32 V3)  
**Author**: OLED UI Explorer (Milestone 1)  
**Date**: 2026-11-15T22:50:00Z  
**Target Code**: `heltec_v3_local_battery\heltec_v3_local_battery.ino`  
**Referenced Contract**: `PROJECT.md` Contract C4 (OLED Header & UI Geometry)  

---

## 1. Observation

Direct code inspection of `c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\heltec_v3_local_battery\heltec_v3_local_battery.ino` and associated headers (`heltec_v3_config.h`, `sdn_header.h`) reveals the following structural, dimensional, and logical issues:

### 1.1 Clumsy Footer Legends (Feature F5)
On all 5 main screen render routines, lines and static instructional footers occupy the bottom 10 vertical pixels ($y = 54..63$):

1. **Screen 0 (HOME)** (`heltec_v3_local_battery.ino:454-456`):
   ```cpp
   display.drawLine(0, 55, 127, 55, SSD1306_WHITE);
   display.setCursor(0, 57);
   display.print("[Mantener PRG: Menu]");
   ```
2. **Screen 1 (BALANZA)** (`heltec_v3_local_battery.ino:476-478`):
   ```cpp
   display.drawLine(0, 54, 127, 54, SSD1306_WHITE);
   display.setCursor(0, 56);
   display.print("[Mantener: Calibrar]");
   ```
3. **Screen 2 (BATERIA)** (`heltec_v3_local_battery.ino:502-504`):
   ```cpp
   display.drawLine(0, 54, 127, 54, SSD1306_WHITE);
   display.setCursor(0, 56);
   display.printf("OLED Sleep: %s", TIMEOUTS_PANTALLA_TXT[timeoutPantallaIdx]);
   ```
4. **Screen 3 (COMMS)** (`heltec_v3_local_battery.ino:524-526`):
   ```cpp
   display.drawLine(0, 54, 127, 54, SSD1306_WHITE);
   display.setCursor(0, 56);
   display.print("[Mantener: Config TX]");
   ```
5. **Screen 4 (SDN)** (`heltec_v3_local_battery.ino:545-547`):
   ```cpp
   display.drawLine(0, 54, 127, 54, SSD1306_WHITE);
   display.setCursor(0, 56);
   display.print("[Mantener: Accion SDN]");
   ```

*Impact*: These footers consume 10 out of 64 vertical pixels (15.6% of the display area). This leaves only 44 vertical pixels ($y = 10..53$) for content, squeezing data into crowded, multi-variable lines that cause severe horizontal text clipping.

---

### 1.2 Text Overflow Faults on 128-Pixel Display (Feature F6)
The SSD1306 display is 128 pixels wide. In Adafruit GFX font size 1, glyphs are 5 pixels wide plus 1 pixel inter-character spacing ($6\text{ px/glyph}$). Maximum horizontal capacity without clipping is:
$$\text{Max Characters} = \left\lfloor \frac{128 - x_{\text{offset}}}{6} \right\rfloor$$
When $x_{\text{offset}} = 0$, capacity is $128 / 6 = 21.33 \to 21$ characters. If $x_{\text{offset}} > 0$, capacity is strictly lower (e.g., at $x=2$, $\le 21$ characters; at $x=12$, $\le 19$ characters; at $x=28$, $\le 16$ characters; at $x=38$, $\le 15$ characters).

The following overflows were directly observed in `heltec_v3_local_battery.ino`:

1. **Boot Splash Screen** (`lines 749-750`):
   ```cpp
   display.setCursor(12, 46);
   display.println("Iniciando LoRaWAN...");
   ```
   Length: 20 characters ($120\text{ px}$). Placed at $x = 12$: $12 + 120 = 132\text{ px} > 128\text{ px}$.
   *Defect*: 4 pixels clipped off the right display border.
2. **Screen 0 (HOME)** (`line 452`):
   ```cpp
   display.printf("Bruto:%.2fkg V:%.3fV", pesoBrutoKg, voltajeSensorV);
   ```
   With nominal sensor values ($18.45\text{ kg}$, $0.922\text{ V}$), rendered string is `"Bruto:18.45kg V:0.922V"` (22 characters = $132\text{ px}$).
   *Defect*: 4 pixels clipped off the right display border (letter `'V'` partially truncated).
3. **Screen 1 (BALANZA)** (`line 474`):
   ```cpp
   display.setCursor(38, 42);
   display.printf("Pend:  %.4f V/k", cal_slope_v_per_kg);
   ```
   With `cal_slope_v_per_kg = 0.0500f`, rendered string is `"Pend:  0.0500 V/k"` (17 characters = $102\text{ px}$). Placed at $x = 38$: $38 + 102 = 140\text{ px} > 128\text{ px}$.
   *Defect*: 12 pixels clipped off the right display border (`" V/k"` severely truncated).
4. **Screen 2 (BATERIA)** (`line 504`):
   ```cpp
   display.printf("OLED Sleep: %s", TIMEOUTS_PANTALLA_TXT[timeoutPantallaIdx]);
   ```
   When `timeoutPantallaIdx == 2` (`"10 Minutos"`), rendered string is `"OLED Sleep: 10 Minutos"` (22 characters = $132\text{ px}$).
   *Defect*: 4 pixels clipped off the right display border.
5. **Screen 3 (COMMS)** (`lines 512-513` and `line 519`):
   - Line 513: `display.setCursor(28, 12); display.printf("RSSI:%ddBm S:%.1f", lora_last_rssi, lora_last_snr);`
     When RSSI is `-85`, rendered string is `"RSSI:-85dBm S:8.0"` (17 characters = $102\text{ px}$). At $x = 28$: $28 + 102 = 130\text{ px} > 128\text{ px}$. 2 pixels clipped.
   - Line 519: `display.printf("ToA: %.1fms SF%u @ %ddBm", current_toa_ms, current_sf, current_tx_power);`
     Rendered string: `"ToA: 82.4ms SF7 @ 14dBm"` (23 characters = $138\text{ px}$).
     *Defect*: 10 pixels clipped off the right display border (`"dBm"` truncated).
6. **Screen 4 (SDN)** (`line 537`):
   ```cpp
   display.printf("CMD : 0x%02X (%s)", sdn_last_cmd, getSdnCommandName(sdn_last_cmd));
   ```
   `getSdnCommandName()` returns:
   - `CMD_SET_TEMP_THRESHOLD`: `"SET_TEMP_THRESHOLD"` (18 chars). Full string: `"CMD : 0x02 (SET_TEMP_THRESHOLD)"` (31 characters = $186\text{ px}$).
   - `CMD_RECONFIG_RADIO`: `"RECONFIG_RADIO"` (14 chars). Full string: `"CMD : 0x01 (RECONFIG_RADIO)"` (27 characters = $162\text{ px}$).
   - `CMD_NOP_KEEP_ALIVE`: `"NOP / KEEP_ALIVE"` (16 chars). Full string: `"CMD : 0x00 (NOP / KEEP_ALIVE)"` (29 characters = $174\text{ px}$).
   *Defect*: Catastrophic overflow between 34 and 58 pixels clipped off screen edge.

---

### 1.3 Technical Header Bar Limitations (Feature F7)
Inspection of `dibujarCabeceraCompacta(int pantalla)` (`lines 365-384`):
```cpp
void dibujarCabeceraCompacta(int pantalla) {
    const char* etiquetas[5] = { "HOME", "BALANZA", "BATERIA", "COMMS", "SDN" };
    
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print(etiquetas[pantalla]);

    const int startX = 86;
    for (int i = 0; i < 5; i++) {
        int cx = startX + (i * 8);
        int cy = 3;
        if (i == pantalla) {
            display.fillCircle(cx, cy, 2, SSD1306_WHITE);
        } else {
            display.drawCircle(cx, cy, 2, SSD1306_WHITE);
        }
    }
    display.drawLine(0, 8, 127, 8, SSD1306_WHITE);
}
```
1. Screen labels `"BALANZA"` and `"BATERIA"` are 7 characters ($42\text{ px}$), taking up excessive horizontal space and preventing the integration of a central clock.
2. No RTC clock (`HH:MM:SS`) is displayed.
3. No LoRaWAN time synchronization status indicator (`*` or `-`) is present.
4. Indicator dots start at $x = 86$, conflicting with the space required for an 8-character time string ($48\text{ px}$) plus sync indicator.
5. The horizontal separator is drawn at $y = 8$, directly colliding with the baseline of size-1 characters whose descenders reach $y = 7..8$.

---

### 1.4 Submenu Modal Architecture Bottlenecks (Feature F8)
Inspection of `renderSubmenu()` and PRG button navigation (`lines 553-579` and `lines 708-712`):
```cpp
// Line 564: Hardcoded limit of 4 options
for (int i = 0; i < totalOpciones && i < 4; i++) {
    int y = 17 + (i * 10);
    ...
}

// Line 709: Hardcoded modulo 4 in PRG navigation
submenuCursor = (submenuCursor + 1) % 4;
```
1. `i < totalOpciones && i < 4`: Explicitly prevents rendering more than 4 items. Passing `totalOpciones = 5` (as required for the 5 ADC sampling intervals) completely ignores the 5th item.
2. Vertical layout: Starting at $y = 17$ with a $10\text{ px}$ pitch means item 4 ($i = 4$) would be drawn at $y = 17 + 40 = 57$, with its highlight box ending at $y = 57 + 9 = 66$, which exceeds the outer modal box ($y = 61$) and the entire screen ($y = 63$).
3. Cursor wrap bug: Hardcoded `(submenuCursor + 1) % 4` prevents the cursor from ever reaching index 4 (item 5). Furthermore, if a submenu had fewer than 4 items, the modulo wrap would cause out-of-bounds cursor positions.

---

### 1.5 Emojis and Non-ASCII Audit (Feature F14)
1. In `heltec_v3_local_battery\heltec_v3_local_battery.ino`: **ZERO** Unicode emojis detected. All comments and strings are pure ASCII.
2. In `heltec_v3_config.h`: Line 41 contains Spanish `ñ` in comment (`señal`), but no emojis.
3. In `sdn_header.h`: Comment lines 25, 35, 41, 43 contain Spanish accented characters (`estándar`, `específicos`, `Avícola`, `calibración`, `transmisión`), but no emojis.
4. In legacy `main\main.ino`: Multiple Unicode emojis were present in serial logs (e.g., U+1F4E5, U+2714, U+274C, U+26A1).

---

## 2. Logic Chain

### 2.1 Footer Removal and Usable Geometry Expansion (F5 $\to$ F6)
1. From Observation 1.1, removing static footers (`[Mantener PRG: Menu]`, etc.) completely clears $y = 54..63$.
2. Header height is defined as $10\text{ px}$ ($y = 0..9$, with separator line at $y = 9$).
3. The resulting usable body region spans $y = 10..63$ (a full $54\text{ vertical pixels}$).
4. In font size 1, a line height of $10\text{ px}$ allows 5 distinct rows ($y = 12, 22, 32, 42, 53$), or 4 rows with a large 2x font metric ($16\text{ px}$ height).
5. This extra vertical space allows breaking cramped lines into separate rows, resolving every single horizontal overflow observed in 1.2 without dropping essential telemetry.

### 2.2 Header Bar Pixel Layout Formulation (F7)
From Observation 1.3 and Contract C4:
- Display width: $128\text{ pixels}$ ($x = 0..127$).
- **Left section ($x = 0..23$)**: Screen name abbreviated to strictly 4 uppercase characters:
  `HOME`, `BALA`, `BATT`, `COMM`, `SDN`. At $6\text{ px/char}$, this consumes exactly $24\text{ pixels}$.
- **Center section ($x = 38..85$)**: RTC time in `"HH:MM:SS"` format ($8\text{ chars} \times 6\text{ px} = 48\text{ pixels}$).
  Centering formula: $\frac{128 - 48}{2} = 40$. Placing cursor at $x = 38$ leaves an even $14\text{ px}$ buffer on the left ($24..37$) and $2\text{ px}$ on the right ($86..87$).
- **Sync indicator ($x = 88..93$)**: Single character `*` if `rtc_sincronizado == true`, else `-` ($6\text{ pixels}$).
- **Right section ($x = 98..126$)**: 5 progress dots representing screens $0..4$.
  - Centers: $cx \in \{100, 106, 112, 118, 124\}$, $cy = 4$, radius $r = 2$.
  - Horizontal bounds: Dot 0 ($x = 98..102$), Dot 4 ($x = 122..126$). Right margin: $1\text{ px}$ ($127$).
  - Selected dot rendered with `fillCircle`, unselected with `drawCircle`.
- **Separator line**: `display.drawLine(0, 9, 127, 9, SSD1306_WHITE)`.
  Positioning at $y = 9$ provides $1\text{ px}$ clearance below glyph bounds ($y = 0..7, 8$). Total header height: $10\text{ px}$ ($y = 0..9$).

### 2.3 Submenu Modal Vertical Geometry and Cursor Modulo (F8)
From Observation 1.4:
- Modal outer box: `(2, 2, 124, 60)`. Inner usable region: $x = 3..125$, $y = 3..61$.
- Modal header: Inverted banner `(3, 3, 122, 10)` with title text at $(6, 4)$.
- For $N \le 5$ options:
  - Vertical step: $y_{\text{step}} = 9\text{ pixels}$ (when $N = 5$) or $10\text{ pixels}$ (when $N \le 4$).
  - Starting coordinate: $y_{\text{start}} = 15$ (when $N = 5$) or $16$ (when $N \le 4$).
  - For $N = 5$:
    - Option 0: $y = 15$, highlight box $y \in [14, 22]$
    - Option 1: $y = 24$, highlight box $y \in [23, 31]$
    - Option 2: $y = 33$, highlight box $y \in [32, 40]$
    - Option 3: $y = 42$, highlight box $y \in [41, 49]$
    - Option 4: $y = 51$, highlight box $y \in [50, 58]$
  - Bottom edge of highlight box 4: $y = 58$. Because $58 \le 61$, all 5 items fit completely inside the dialog box with $3\text{ px}$ margin at the bottom.
- Cursor wrap fix:
  Maintain a dynamic variable `int totalSubmenuOpciones` set directly by `renderSubmenu()`. In `gestionarBotonPRG()`, update cursor via:
  ```cpp
  if (totalSubmenuOpciones > 0) {
      submenuCursor = (submenuCursor + 1) % totalSubmenuOpciones;
  }
  ```
  This guarantees that 5-item lists cycle through $0, 1, 2, 3, 4, 0\dots$ without index starvation or out-of-bounds selection.

### 2.4 Overflow Elimination by Screen (F6)
- **Splash Screen**: Left margin shifted from $x = 12$ to $x = 4$. `"Iniciando LoRaWAN..."` ($120\text{ px}$) spans $x = 4..124 \le 128\text{ px}$. Zero overflow.
- **Screen 0 (HOME)**: Split `"Bruto:%.2fkg V:%.3fV"` into `"B:%.2fkg V:%.2fV"` ($17\text{ chars} = 102\text{ px} \le 128\text{ px}$). Add technical status at $y = 53$: `"TX:%lus | ADC:%s"` ($15\text{ chars} = 90\text{ px}$).
- **Screen 1 (BALANZA)**: Move scale vector to $x = 2$. Right text at $x = 34$: `"Neto: %.2fkg"` ($13\text{ chars}$), `"Bruto:%.2fkg"` ($13\text{ chars}$), `"V_adc:%.3fV"` ($12\text{ chars}$), `"Pend: %.4f"` ($12\text{ chars} = 72\text{ px} \to 34 + 72 = 106 \le 128$). At $y = 53$: `"Tara: %.2fkg | ADC:OK"` ($20\text{ chars} = 120\text{ px}$ at $x = 2 \to 122 \le 128$). Zero overflow.
- **Screen 2 (BATERIA)**: Pack info at $y = 33$: `"Vbat: %.2fV (400mAh)"` ($20\text{ chars} = 120\text{ px}$). Autonomy at $y = 43$: `"Autonomia: ~%dh %02dm"` ($19\text{ chars} = 114\text{ px}$). Status line at $y = 53$: `"OLED: %s | ADC: %s"` ($18\text{ chars} = 108\text{ px}$). Zero overflow.
- **Screen 3 (COMMS)**: Antena text at $x = 26$: `"R:%ddBm S:%.1f"` ($14\text{ chars} = 84\text{ px} \to 26 + 84 = 110 \le 128$). Split ToA and RF: Line 3 at $y = 32$: `"SF%u @ %ddBm (DR%u)"` ($17\text{ chars} = 102\text{ px}$). Line 4 at $y = 42$: `"ToA: %.1fms | TX: %lus"` ($20\text{ chars} = 120\text{ px}$). Line 5 at $y = 53$: `"Banda: US915 SubB2"` ($18\text{ chars} = 108\text{ px}$). Zero overflow.
- **Screen 4 (SDN)**: Implement `getSdnCommandShortName(uint8_t cmd)`. Line 2 at $y = 22$: `"CMD: 0x%02X [%s]"` (max $20\text{ chars} = 120\text{ px}$). Line 3 at $y = 32$: `"CFG: SF%u@%ddBm %lus"` ($18\text{ chars} = 108\text{ px}$). Line 4 at $y = 42$: `"RAW: 0x%08X"` ($15\text{ chars} = 90\text{ px}$). Line 5 at $y = 53$: `"FPort: 10 (204B SDN)"` ($20\text{ chars} = 120\text{ px}$). Zero overflow.

---

## 3. Caveats

1. **Internal RTC Time Drift**:
   The Heltec WiFi LoRa 32 V3 relies on the internal RTC timer of the ESP32-S3. Without network synchronization, it drifts approximately 1-2% per day. Before synchronization with ChirpStack v4 via `DeviceTimeReq`, the clock displays time elapsed since boot (`00:00:xx`) and the sync flag displays `-`. Once `DeviceTimeAns` is received, `rtc_sincronizado` becomes `true` and the flag flips to `*`.
2. **Adafruit GFX Font Metric Assumption**:
   All string length calculations assume the default Adafruit GFX 6x8 monospace font (`setTextSize(1)`), which has a 5-pixel glyph width and a 1-pixel right margin. If a custom proportional font is loaded, horizontal pixel widths must be recalculated using `getTextBounds()`.
3. **Screen Refresh Rate vs Clock Rollover**:
   The OLED display refreshes every $100\text{ ms}$ (`millis() - ultimoRefresco >= 100`). This ensures the RTC seconds display updates within $100\text{ ms}$ of the internal clock second transition without perceptible stutter or display tearing.
4. **Read-Only Investigation Boundary**:
   In accordance with the Teamwork Explorer contract, no modifications have been directly applied to the repository source files. The exact proposed code blocks in Section 4 are formulated for immediate application by the Milestone 1 Worker agent.

---

## 4. Conclusion

### 4.1 Summary of Architecture & Parameter Changes
| Feature | Parameter / Component | Baseline | Proposed Design |
|---|---|---|---|
| **F5** | Footer legends across screens | Present (`[Mantener...]`, lines at $y=54..55$) | **Completely Removed**; $y=54..63$ freed for telemetry |
| **F6** | Max characters per line ($x=0$) | Up to 31 chars (severe overflow) | **Strictly $\le 21$ characters** across all screens |
| **F6** | Boot Splash Screen | $x=12$ (4 px overflow on line 3) | **$x=4$** (0 px overflow; $124\text{ px} \le 128\text{ px}$) |
| **F7** | Header bar dimensions | Height $8\text{ px}$, line at $y=8$, no clock | **Height $10\text{ px}$ ($y=0..9$), line at $y=9$** |
| **F7** | Screen name labels | 4 to 7 chars (`"BALANZA"`, `"BATERIA"`) | **Strictly 4 chars** (`"HOME"`, `"BALA"`, `"BATT"`, `"COMM"`, `"SDN"`) |
| **F7** | Timekeeping & sync flag | None | **`HH:MM:SS` ($x=38$) + `*`/`-` ($x=88$)** |
| **F7** | Screen progress indicators | Dots at $x=86$ (step 8 px) | **5 dots at $cx=100..124$ ($r=2$, step 6 px)** |
| **F8** | Modal item capacity | Hardcoded 4 items max ($i < 4$) | **Dynamic $1..5$ items** ($y_{\text{step}}=9\text{ px}$, $y_{\text{start}}=15$) |
| **F8** | Submenu cursor wrap | Hardcoded `(cursor + 1) % 4` | **Dynamic `(cursor + 1) % totalSubmenuOpciones`** |
| **F14** | Zero Emojis compliance | Emojis in legacy v1.0 | **100% pure ASCII strings and icons across all files** |

---

### 4.2 Exact Proposed Code Modifications for `heltec_v3_local_battery.ino`

#### Modification 1: Global State Tracking & SDN Helper
*Insert before section 5 (Iconografia Vectorial)*:
```cpp
// --- Variable para navegacion dinamica de submenus (F8) ---
int totalSubmenuOpciones = 4;

// --- Helper para nombres cortos de comandos SDN sin overflow (F6) ---
inline const char* getSdnCommandShortName(uint8_t cmd) {
    switch (cmd) {
        case CMD_NOP_KEEP_ALIVE:     return "NOP";
        case CMD_RECONFIG_RADIO:     return "RECONFIG";
        case CMD_SET_TEMP_THRESHOLD: return "TEMP_TH";
        case CMD_FORCE_SLEEP:        return "SLEEP";
        case CMD_SET_P2P_RELAY:      return "RELAY";
        case CMD_SYSTEM_RESET:       return "RESET";
        default:                     return "UNKNOWN";
    }
}
```

---

#### Modification 2: Redesigned Technical Header Bar (Feature F7)
*Replace `dibujarCabeceraCompacta` (`lines 365-384`)*:
```cpp
void dibujarCabeceraCompacta(int pantalla) {
    static const char* const ETIQUETAS[5] = { "HOME", "BALA", "BATT", "COMM", "SDN" };
    const char* etiqueta = (pantalla >= 0 && pantalla < 5) ? ETIQUETAS[pantalla] : "MENU";

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    // 1. Identificador de 4 caracteres (x=0..23, y=0..7)
    display.setCursor(0, 0);
    display.print(etiqueta);

    // 2. Reloj RTC interno HH:MM:SS (x=38..85, y=0..7)
    char relojBuf[10];
    obtenerHoraFormateada(relojBuf, sizeof(relojBuf));
    display.setCursor(38, 0);
    display.print(relojBuf);

    // 3. Indicador de sincronizacion LoRaWAN (x=88, y=0..7)
    display.setCursor(88, 0);
    display.print(rtc_sincronizado ? '*' : '-');

    // 4. 5 Puntos indicadores de progreso (cx = 100, 106, 112, 118, 124; cy = 4)
    for (int i = 0; i < 5; i++) {
        int cx = 100 + (i * 6);
        int cy = 4;
        if (i == pantalla) {
            display.fillCircle(cx, cy, 2, SSD1306_WHITE);
        } else {
            display.drawCircle(cx, cy, 2, SSD1306_WHITE);
        }
    }

    // 5. Linea separadora a y=9 (Cabecera ocupa y=0..9, cuerpo inicia en y=10)
    display.drawLine(0, 9, 127, 9, SSD1306_WHITE);
}
```

---

#### Modification 3: Redesigned Screen Bodies (Features F5 & F6)
*Replace `renderPantallaHome` through `renderPantallaSDN` (`lines 434-549`)*:
```cpp
void renderPantallaHome() {
    dibujarCabeceraCompacta(0);

    // Fila 1 (y=12): Antena, estado de conexion y bateria mini con porcentaje
    dibujarAntena(2, 11, lora_last_rssi);
    display.setTextSize(1);
    display.setCursor(24, 12);
    display.print(lora_joined ? "CONECT" : "BUSC");

    dibujarMiniBateria(84, 12, porcentajeBateria);
    display.setCursor(102, 12);
    display.printf("%d%%", porcentajeBateria);

    // Fila 2 (y=24): Peso principal en fuente grande (Size 2)
    display.setTextSize(2);
    display.setCursor(6, 24);
    display.printf("%.2f kg", pesoNetoKg);

    // Fila 3 (y=43): Datos secundarios sin overflow (17 caracteres = 102 px <= 128 px)
    display.setTextSize(1);
    display.setCursor(2, 43);
    display.printf("B:%.2fkg V:%.2fV", pesoBrutoKg, voltajeSensorV);

    // Fila 4 (y=53): Resumen tecnico de TX y muestreo ADC (15 caracteres = 90 px <= 128 px)
    display.setCursor(2, 53);
    display.printf("TX:%lus | ADC:%s", PERIODOS_TX_S[periodoTxIdx], 
                   (modoMuestreoAdcIdx == 0 ? "TX" : 
                   (modoMuestreoAdcIdx == 1 ? "10s" : 
                   (modoMuestreoAdcIdx == 2 ? "30s" : 
                   (modoMuestreoAdcIdx == 3 ? "60s" : "300s")))));
}

void renderPantallaBalanza() {
    dibujarCabeceraCompacta(1);
    dibujarIconoBalanza(2, 13);

    // Filas laterales derecha (x=34): Max 15 caracteres (90 px)
    display.setTextSize(1);
    display.setCursor(34, 12);
    display.printf("Neto: %.2fkg", pesoNetoKg);

    display.setCursor(34, 22);
    display.printf("Bruto:%.2fkg", pesoBrutoKg);

    display.setCursor(34, 32);
    display.printf("V_adc:%.3fV", voltajeSensorV);

    display.setCursor(34, 42);
    display.printf("Pend: %.4f", cal_slope_v_per_kg);

    // Fila inferior (y=53): 20 caracteres = 120 px <= 128 px
    display.setCursor(2, 53);
    display.printf("Tara: %.2fkg | ADC:OK", cal_tare_kg);
}

void renderPantallaBateria() {
    dibujarCabeceraCompacta(2);

    // Indicador grafico grande de bateria y porcentaje
    display.drawRoundRect(2, 12, 42, 18, 2, SSD1306_WHITE);
    display.fillRect(44, 16, 3, 10, SSD1306_WHITE);
    int fillW = map(porcentajeBateria, 0, 100, 0, 36);
    if (fillW > 0) display.fillRect(5, 15, fillW, 12, SSD1306_WHITE);

    display.setTextSize(2);
    display.setCursor(54, 13);
    display.printf("%d%%", porcentajeBateria);

    // Fila 2 (y=33): Tension exacta del paquete LiPo (20 caracteres = 120 px <= 128 px)
    display.setTextSize(1);
    display.setCursor(2, 33);
    display.printf("Vbat: %.2fV (400mAh)", voltajeBateriaFiltrado);

    // Fila 3 (y=43): Estimacion de tiempo restante (19 caracteres = 114 px <= 128 px)
    display.setCursor(2, 43);
    int h = (int)autonomiaHoras;
    int m = (int)((autonomiaHoras - h) * 60.0f);
    display.printf("Autonomia: ~%dh %02dm", h, m);

    // Fila 4 (y=53): Timeout de pantalla e intervalo ADC (18 caracteres = 108 px <= 128 px)
    display.setCursor(2, 53);
    display.printf("OLED: %s | ADC: %s",
                   (timeoutPantallaIdx == 0 ? "1m" : (timeoutPantallaIdx == 1 ? "5m" : (timeoutPantallaIdx == 2 ? "10m" : "ON"))),
                   (modoMuestreoAdcIdx == 0 ? "TX" : (modoMuestreoAdcIdx == 1 ? "10s" : (modoMuestreoAdcIdx == 2 ? "30s" : (modoMuestreoAdcIdx == 3 ? "60s" : "300s")))));
}

void renderPantallaComms() {
    dibujarCabeceraCompacta(3);

    // Fila 1 (y=12): Antena y niveles RF (14 caracteres = 84 px -> 26 + 84 = 110 px <= 128 px)
    dibujarAntena(2, 11, lora_last_rssi);
    display.setTextSize(1);
    display.setCursor(26, 12);
    display.printf("R:%ddBm S:%.1f", lora_last_rssi, lora_last_snr);

    // Fila 2 (y=22): Contadores de paquetes (15 caracteres = 90 px <= 128 px)
    display.setCursor(2, 22);
    display.printf("TX: %lu | RX: %lu", paquetesTX, paquetesRX);

    // Fila 3 (y=32): Parametros LoRaWAN SF, potencia y datarate (17 caracteres = 102 px <= 128 px)
    display.setCursor(2, 32);
    display.printf("SF%u @ %ddBm (DR%u)", current_sf, current_tx_power, lora_datarate);

    // Fila 4 (y=42): Tiempo en el aire e intervalo TX (20 caracteres = 120 px <= 128 px)
    display.setCursor(2, 42);
    display.printf("ToA: %.1fms | TX: %lus", current_toa_ms, PERIODOS_TX_S[periodoTxIdx]);

    // Fila 5 (y=53): Region de frecuencia (18 caracteres = 108 px <= 128 px)
    display.setCursor(2, 53);
    display.printf("Banda: US915 SubB2");
}

void renderPantallaSDN() {
    dibujarCabeceraCompacta(4);

    // Fila 1 (y=12): Contador de tramas SDN (19 caracteres = 114 px <= 128 px)
    display.setTextSize(1);
    display.setCursor(0, 12);
    display.printf("RX SDN #%lu (Gateway)", sdn_rx_count);

    // Fila 2 (y=22): Comando decodificado con nombre corto (max 20 caracteres = 120 px <= 128 px)
    display.setCursor(0, 22);
    display.printf("CMD: 0x%02X [%s]", sdn_last_cmd, getSdnCommandShortName(sdn_last_cmd));

    // Fila 3 (y=32): Parametros SDN aplicados (18 caracteres = 108 px <= 128 px)
    display.setCursor(0, 32);
    display.printf("CFG: SF%u@%ddBm %lus", sdn_last_sf, sdn_last_pwr, sdn_last_rate_s);

    // Fila 4 (y=42): Palabra cruda de control SDN (15 caracteres = 90 px <= 128 px)
    display.setCursor(0, 42);
    display.printf("RAW: 0x%08X", sdn_last_raw_word);

    // Fila 5 (y=53): Metadatos del protocolo SDN (20 caracteres = 120 px <= 128 px)
    display.setCursor(0, 53);
    display.printf("FPort: 10 (204B SDN)");
}
```

---

#### Modification 4: Overhauled Dynamic Submenu Modal (Feature F8)
*Replace `renderSubmenu` (`lines 553-579`)*:
```cpp
void renderSubmenu(const char* titulo, const char* opciones[], int totalOpciones, int seleccionado) {
    int count = totalOpciones;
    if (count > 5) count = 5;
    if (count < 1) count = 1;
    totalSubmenuOpciones = count;

    if (seleccionado >= count) {
        seleccionado = 0;
        submenuCursor = 0;
    }

    // Marco exterior modal (124x60 px)
    display.fillRect(2, 2, 124, 60, SSD1306_BLACK);
    display.drawRect(2, 2, 124, 60, SSD1306_WHITE);

    // Cabecera invertida del modal (122x10 px)
    display.fillRect(3, 3, 122, 10, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setTextSize(1);
    display.setCursor(6, 4);
    display.print(titulo);
    display.setTextColor(SSD1306_WHITE);

    // Espaciado vertical dinamico segun cantidad de opciones (hasta 5 items)
    int y_step = (count >= 5) ? 9 : 10;
    int y_start = (count >= 5) ? 15 : 16;

    for (int i = 0; i < count; i++) {
        int y = y_start + (i * y_step);
        if (i == seleccionado) {
            display.fillRect(5, y - 1, 118, y_step, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK);
            display.setCursor(7, y);
            display.print(">");
            display.setCursor(15, y);
            display.print(opciones[i]);
            display.setTextColor(SSD1306_WHITE);
        } else {
            display.setCursor(15, y);
            display.print(opciones[i]);
        }
    }
}
```

---

#### Modification 5: Safe Cursor Navigation & Modulo Wrap (Feature F8)
*Update `gestionarBotonPRG` (`lines 708-712`)*:
```cpp
            if (!submenuActivo) {
                pantallaActual = (Pantalla)((pantallaActual + 1) % TOTAL_PANTALLAS);
            } else {
                if (totalSubmenuOpciones > 0) {
                    submenuCursor = (submenuCursor + 1) % totalSubmenuOpciones;
                } else {
                    submenuCursor = 0;
                }
                submenuUltimaActividad = millis();
            }
```

---

#### Modification 6: Boot Splash Screen Alignment Fix (Feature F6)
*Update `setup()` (`lines 745-751`)*:
```cpp
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(4, 16);
    display.println("BALANZA AVICOLA");
    display.setCursor(4, 30);
    display.println("Firmware Bateria v2");
    display.setCursor(4, 44);
    display.println("Iniciando LoRaWAN...");
    display.display();
```

---

## 5. Verification Method

### 5.1 Compilation Verification Commands
Execute compilation of the battery firmware using `arduino-cli`:
```powershell
$cli = "C:\Users\mate1\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
$fqbn = "esp32:esp32:heltec_wifi_lora_32_V3"

& $cli compile --fqbn $fqbn "c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\heltec_v3_local_battery\heltec_v3_local_battery.ino"
```

### 5.2 Verification Inspection Matrix
1. **Header Bar Metrics**:
   - Inspect `dibujarCabeceraCompacta`: Ensure separator line is at $y = 9$, screen names are 4 characters, clock is at $x = 38$, sync flag is at $x = 88$, and 5 dots are centered at $cx \in \{100, 106, 112, 118, 124\}$ with $cy = 4$.
2. **Horizontal Glyph Capacity**:
   - Inspect all strings passed to `display.print` or `display.printf`: Verify that $(x_{\text{cursor}} + (\text{string\_length} \times 6)) \le 128\text{ pixels}$.
3. **Submenu 5-Item Boundary Test**:
   - Verify `renderSubmenu`: Highlight box for item 4 ends at $y = 58 \le 61$.
   - Verify `gestionarBotonPRG`: Cycle PRG button 5 times in a 5-item menu to confirm sequence $0 \to 1 \to 2 \to 3 \to 4 \to 0$.
4. **Zero Emojis Automated Audit**:
   ```powershell
   Get-ChildItem -Path "c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\heltec_v3_local_battery" -Recurse -File | ForEach-Object {
       $lines = Get-Content $_.FullName
       for ($i = 0; $i -lt $lines.Count; $i++) {
           if ($lines[$i] -match "[\uD83C-\uDBFF\uDC00-\uDFFF\u2600-\u27BF]") {
               Write-Error "Emoji detected in $($_.Name) line $($i+1): $($lines[$i])"
           }
       }
   }
   ```

### 5.3 Invalidation Conditions
- Any occurrence of Unicode emojis in source code, headers, OLED buffers, or comments invalidates compliance.
- Any text string in font size 1 exceeding 21 characters on a line (or exceeding $(128 - x)/6$ when offset) invalidates compliance.
- Submenu cursor locking, skipping, or overflowing when rendering 5 options invalidates compliance.
- Any modification to files inside the protected `main/` directory invalidates compliance.
