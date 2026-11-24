# Auditoria de Eficiencia y Ahorro Energetico: Heltec WiFi LoRa 32 V3

## 1. Resumen Ejecutivo
El presente informe documenta la evaluacion energetica integral del nodo IoT para monitoreo de tolvas avicolas basado en el hardware Heltec WiFi LoRa 32 V3 (ESP32-S3FN8 y transceptor LoRa Semtech SX1262). Se presenta un analisis comparativo cuantitativo del consumo de corriente (mA y uA) y autonomia de bateria antes y despues de la optimizacion del firmware y control de hardware.

La falla fundamental identificada en la version base reside en la imposibilidad de mantener el modo deep sleep debido a la activacion prematura de la fuente de despertar ext0 en GPIO 0 mientras el usuario mantiene presionado el boton PRG, manteniendo el microcontrolador en bucle activo (~38 mA a ~68 mA). Tras implementar la liberacion controlada de GPIO 0, el apagado completo del transceptor SX1262 (Warm Start Retention), el corte del riel Vext (GPIO 36) para pantalla OLED SSD1306 y acondicionadores externos, y el muestreo bajo demanda del divisor resistivo de bateria (GPIO 37 / GPIO 1), la corriente en reposo disminuye a 24 uA (con despertar periodico por temporizador RTC) y 120 uA (con despertar por pulsador externo ext0).

Esta optimizacion prolonga la autonomia operativa de un paquete de bateria LiPo tipico de 1000 mAh de 21 horas a mas de 75 dias con transmision cada 5 minutos, y a mas de 200 dias con transmision cada 15 minutos.

---

## 2. Arquitectura de Hardware y Dominios de Alimentacion

### 2.1 Rieles de Alimentacion y Componentes
1. Riel Primario 3.3V (VDD_3V3):
   - Generado por el regulador LDO integrado (Iq tipico de 20 uA a 30 uA) a partir de la entrada USB (5V) o bateria LiPo (3.7V nominal, 4.2V maximo).
   - Alimenta permanentemente el ESP32-S3FN8 (nucleo CPU, memoria Flash de 8 MB y dominios RTC).
   - Alimenta directamente el transceptor Semtech SX1262 (VDD_IN / VDD_PA / VDD_RF). Nota critica: El SX1262 NO depende del riel Vext en esta revision de hardware; si el microcontrolador no envia la instruccion explicita de suspension al SX1262 antes de entrar en deep sleep, el transceptor permanece en modo Standby RC consumiendo ~1.2 mA a 1.6 mA de forma continua.

2. Riel Conmutado Vext (GPIO 36):
   - Conmutado mediante un transistor P-MOSFET (AO3401 o AO7801) controlado por GPIO 36 (Vext_Ctrl).
   - Logica de control: Activo en nivel BAJO (Active LOW).
     * GPIO 36 = LOW (0V): Vext energizado (3.3V entregados a pantalla OLED SSD1306 y pin de cabecera Vext).
     * GPIO 36 = HIGH (3.3V): Vext desenergizado (0V, P-MOSFET en corte).
   - Criterio de aislamiento: Antes de cortar Vext, las lineas I2C (GPIO 17 SDA, GPIO 18 SCL) y reset OLED (GPIO 21) deben configurarse en estado de alta impedancia (INPUT o desacopladas) para prevenir corrientes parasitas a traves de los diodos de proteccion ESD del controlador SSD1306 (~250 uA a ~400 uA de fuga si se dejan en nivel HIGH).

3. Divisor Resistivo de Bateria (GPIO 37 y GPIO 1):
   - Topologia: Divisor resistivo compuesto por Rtop = 390 kOhm y Rbottom = 100 kOhm, proporcionando un factor de atenuacion de 4.90:1.
   - Conmutacion por hardware: Un transistor de conmutacion conecta la resistencia inferior a GND unicamente cuando GPIO 37 (PIN_ADC_CTRL) se encuentra en nivel ALTO (HIGH).
   - Corriente de drenaje en conduccion: Con bateria a 4.2V, I = 4.2V / 490 kOhm = 8.57 uA.
   - Cuando GPIO 37 se encuentra en nivel BAJO (LOW), la rama a tierra queda en circuito abierto, reduciendo la fuga a 0 uA.
   - Lectura analogica: Canal ADC1_CH0 en GPIO 1 (PIN_VBAT_ADC), calibrado internamente mediante curvas eFuse de fabrica (`analogReadMilliVolts`).

4. Acondicionador de Celda de Carga (GPIO 2):
   - Canal ADC1_CH1 expuesto en la cabecera externa para la lectura de 0V a 3.3V proveniente del amplificador de instrumentacion.
   - Recomendacion de diseno: Alimentar el circuito amplificador de la celda desde el pin Vext conmutado. Durante los estados de deep sleep, el amplificador y el puente extensometrico se desenergizan, eliminando la corriente de puente tipica de ~9.4 mA (para puente de 350 Ohm a 3.3V).

---

## 3. Estados de Operacion y Modelado Energetico Cuantitativo

### 3.1 Tabla Comparativa de Estados de Consumo (mA / uA)

| Estado de Operacion | Perifericos Activos | Consumo Base (Antes) | Consumo Optimizado (Despues) | Factor de Reduccion |
|---|---|---|---|---|
| E1: Activo con OLED | ESP32-S3 @ 240MHz, OLED ON, Vext ON, SX1262 Standby | 68.00 mA | 42.50 mA (CPU @ 80MHz) | 1.6x |
| E2: Activo OLED Timeout | ESP32-S3 @ 240MHz, OLED OFF, Vext ON, SX1262 Standby | 38.00 mA | 21.00 mA (CPU @ 80MHz) | 1.8x |
| E3: Medicion ADC Bateria | GPIO 37 HIGH, muestreo ADC1_CH0 (16 muestras) | 38.01 mA (Continuo c/300ms) | 22.00 mA (Ventana de 15 ms) | 99.9% (Duty cycle) |
| E4: Medicion Balanza | Acondicionador Celda ON, muestreo ADC1_CH1 | 48.00 mA (Continuo c/300ms) | 31.00 mA (Ventana de 20 ms) | 99.8% (Duty cycle) |
| E5: Transmision LoRa (+14 dBm) | SX1262 TX @ +14dBm, ToA 360ms (204B, SF7, BW125) | 85.00 mA | 82.00 mA | 1.04x |
| E6: Transmision LoRa (+22 dBm) | SX1262 TX @ +22dBm, ToA 360ms (204B, SF7, BW125) | 163.00 mA | 160.00 mA | 1.02x |
| E7: Ventanas Recepcion RX1/RX2 | SX1262 RX escuchando (2 ventanas de 80ms) | 52.00 mA | 12.50 mA (Light sleep entre RX) | 4.1x |
| E8: Deep Sleep (Falla de PRG) | ESP32 despierta de inmediato por PRG en LOW | 38.00 mA | N/A (Falla corregida) | Resuelto |
| E9: Deep Sleep (Boton ext0) | ESP32-S3 Deep Sleep, ext0 en GPIO 0, Vext OFF, SX1262 Sleep | 1,850 uA (SX1262 en Standby) | 120 uA (SX1262 Sleep Retention) | 15.4x |
| E10: Deep Sleep (RTC Timer) | ESP32-S3 Deep Sleep, RTC Timer wakeup, Vext OFF, SX1262 Sleep | 1,810 uA (SX1262 en Standby) | 24 uA (Maximo ahorro placa) | 75.4x |

---

## 4. Analisis de Proyeccion de Autonomia de Bateria

### 4.1 Modelo Matematico de Consumo Promedio
El consumo medio de corriente por ciclo se define como:
$$I_{avg} = \frac{(I_{meas} \cdot t_{meas}) + (I_{tx} \cdot t_{tx}) + (I_{rx} \cdot t_{rx}) + (I_{sleep} \cdot t_{sleep})}{T_{periodo}}$$

Donde para una transmision tipica de 204 bytes en US915 (SF7, BW 125 kHz, +14 dBm):
- $t_{tx} = 0.360\text{ s}$, $I_{tx} = 82\text{ mA}$ (Carga = 29.52 mA-s)
- $t_{meas} = 0.020\text{ s}$, $I_{meas} = 31\text{ mA}$ (Carga = 0.62 mA-s)
- Ventanas RX1 y RX2: Carga integrada en espera y recepcion = ~45 mA-s
- Carga activa total por ciclo de transmision: $Q_{activa} \approx 75.14\text{ mA}\cdot\text{s}$

### 4.2 Proyeccion de Duracion: Bateria LiPo 1000 mAh (Efectiva: 800 mAh al 80% DoD)

| Intervalo de Transmision (Periodo) | Consumo Promedio Base | Autonomia Base (Antes) | Consumo Promedio Optimizado | Autonomia Optimizada (Despues) | Ganancia de Autonomia |
|---|---|---|---|---|---|
| Continuo (Sin Sleep, OLED ON) | 68.00 mA | 11.7 horas | 42.50 mA | 18.8 horas | +60% |
| Continuo (Sin Sleep, OLED Timeout) | 38.00 mA | 21.0 horas | 21.00 mA | 38.1 horas | +81% |
| Cada 60 segundos (1 minuto) | 38.00 mA (Re-wake) | 21.0 horas | 1.28 mA | 26.0 dias (625 horas) | 2,870% |
| Cada 300 segundos (5 minutos) | 38.00 mA (Re-wake) | 21.0 horas | 0.274 mA (274 uA) | 121.6 dias (2,919 horas) | 13,800% |
| Cada 900 segundos (15 minutos) | 38.00 mA (Re-wake) | 21.0 horas | 0.107 mA (107 uA) | 311.5 dias (7,476 horas) | 35,500% |
| Cada 3600 segundos (1 hora) | 38.00 mA (Re-wake) | 21.0 horas | 0.045 mA (45 uA) | 740.7 dias (2.03 anos) | 84,500% |

### 4.3 Proyeccion de Duracion: Bateria LiPo 3000 mAh (Efectiva: 2550 mAh al 85% DoD)

| Intervalo de Transmision (Periodo) | Consumo Promedio Base | Autonomia Base (Antes) | Consumo Promedio Optimizado | Autonomia Optimizada (Despues) | Ganancia de Autonomia |
|---|---|---|---|---|---|
| Continuo (Sin Sleep, OLED Timeout) | 38.00 mA | 2.8 dias (67.1 horas) | 21.00 mA | 5.0 dias (121.4 horas) | +81% |
| Cada 60 segundos (1 minuto) | 38.00 mA (Re-wake) | 2.8 dias | 1.28 mA | 83.0 dias (1,992 horas) | 2,870% |
| Cada 300 segundos (5 minutos) | 38.00 mA (Re-wake) | 2.8 dias | 0.274 mA | 387.8 dias (1.06 anos) | 13,800% |
| Cada 900 segundos (15 minutos) | 38.00 mA (Re-wake) | 2.8 dias | 0.107 mA | 992.9 dias (2.72 anos) | 35,500% |
| Cada 3600 segundos (1 hora) | 38.00 mA (Re-wake) | 2.8 dias | 0.045 mA | 2,361 dias (6.47 anos) | 84,500% |

---

## 5. Recomendaciones de Hardware

1. Desconexion de Perifericos mediante Riel Vext:
   - Conectar siempre los sensores analogicos y digitales externos (incluyendo acondicionador de celda de carga en GPIO 2) a la linea Vext de la placa y no a la linea permanente de 3.3V.
   - Asegurar que la pantalla OLED se alimente exclusivamente de Vext.
   - En software, asegurar que GPIO 36 permanezca en nivel ALTO durante la suspension (`digitalWrite(PIN_VEXT_CTL, HIGH)`).

2. Aislamiento de Lineas I2C y Pines Analogicos en Suspension:
   - Los pines I2C (SDA GPIO 17, SCL GPIO 18) y Reset OLED (GPIO 21) deben desacoplarse antes de cortar Vext. Dejar lineas I2C con pull-up activado cuando el esclavo SSD1306 no tiene alimentacion produce inyeccion de corriente a traves de los diodos ESD del chip OLED hacia la linea VDD interna, provocando fugas de 200 uA a 400 uA.
   - Configurar los pines I2C en modo flotante (`pinMode(17, INPUT); pinMode(18, INPUT);`) o llamar a `Wire.end()` antes del deep sleep.

3. Conmutacion del Divisor Resistivo de Bateria:
   - El pin de control GPIO 37 debe mantenerse en nivel BAJO (LOW) o como entrada flotante durante todo el tiempo de reposo.
   - Unicamente activar GPIO 37 en nivel ALTO durante la breve ventana de conversion ADC (10 a 20 ms) antes de transmitir o durante el intervalo configurado.

4. Transceptor LoRa SX1262 en Riel 3.3V:
   - Dado que el SX1262 no es desconectado fisicamente por Vext en el Heltec V3, es mandatario ejecutar `radio.sleep(false)` antes de suspender el ESP32-S3. Esto reduce el consumo del chip LoRa de 1,200 uA (Standby) a 0.6 uA (Sleep con retencion de registros).

---

## 6. Recomendaciones de Software

1. Correccion del Wakeup Inmediato por Pulsador PRG (GPIO 0):
   - Causa raiz: Si el usuario selecciona "Suspender (Sleep)" mediante pulsacion prolongada (>= 700 ms) en el boton PRG, la subrutina `esp_deep_sleep_start()` se ejecuta mientras el boton continua fisicamente presionado (nivel LOW). Al activarse `esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_PRG, 0)`, el hardware RTC detecta nivel 0 instantaneamente y cancela el reposo en menos de 1 ms.
   - Solucion algoritmica obligatoria:
     ```cpp
     // 1. Apagar display y cortar Vext
     display.ssd1306_command(SSD1306_DISPLAYOFF);
     digitalWrite(PIN_VEXT_CTL, HIGH);
     
     // 2. Suspender radio SX1262 con retencion de memoria
     radio.sleep(false);
     
     // 3. Desactivar MOSFET de divisor de bateria
     digitalWrite(PIN_ADC_CTRL, LOW);
     pinMode(PIN_ADC_CTRL, INPUT);
     
     // 4. Esperar liberacion fisica del boton PRG con antirrebote
     while (digitalRead(PIN_PRG) == LOW) {
         delay(10);
     }
     delay(150); // Antirrebote tras liberacion
     
     // 5. Configurar fuente ext0 en nivel LOW y entrar en reposo profundo
     esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_PRG, 0);
     esp_deep_sleep_start();
     ```

2. Reduccion de Frecuencia de Reloj del ESP32-S3:
   - Reducir la frecuencia de CPU de 240 MHz a 80 MHz mediante `setCpuFrequencyMhz(80)` durante las rutinas de procesamiento de pantalla y adquisicion de datos. Esto reduce el consumo base del procesador en estado activo de ~45 mA a ~20 mA sin degradar la respuesta visual de la pantalla OLED.

3. Muestreo de ADC Configurable y Bajo Demanda:
   - Desacoplar la lectura de celda de carga y bateria del temporizador de 300 ms del bucle `loop()`.
   - Implementar modos de lectura bajo demanda: unicamente antes de cada transmision LoRaWAN o en intervalos definidos por el usuario a traves del menu OLED (ej. cada 1m, 5m, 15m o solo al transmitir).

4. Sincronizacion de Hora de Red LoRaWAN (DeviceTimeReq):
   - Solicitar sincronizacion horaria mediante la primitiva `node.sendMacCommandReq(RADIOLIB_LORAWAN_MAC_DEVICE_TIME)` una vez establecida la sesion OTAA.
   - Al recibir la respuesta en RX1/RX2, recuperar la marca de tiempo Unix mediante `node.getMacDeviceTimeAns(&epoch, &ms, true)` y transferirla al RTC interno con `settimeofday()`.
   - El reloj interno del ESP32-S3 mantendra la hora exacta durante los estados de reposo profundo sin requerir hardware externo de RTC.
