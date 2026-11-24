#ifndef SCALE_DRIVER_H
#define SCALE_DRIVER_H

#include <Arduino.h>
#include <Preferences.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "heltec_v3_config.h"

// Dimensiones de la pantalla OLED integrada del Heltec V3
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

class ScaleDriver {
public:
    ScaleDriver() : display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, PIN_OLED_RST) {}

    // Parámetros de calibración actuales
    float cal_slope_v_per_kg = DEFAULT_CAL_SLOPE_V_PER_KG;
    float cal_zero_v         = DEFAULT_CAL_ZERO_V;
    float cal_tare_kg        = 0.0f;
    float hopper_capacity_kg = DEFAULT_HOPPER_CAPACITY_KG;

    // Lecturas filtradas en tiempo real
    float    live_voltage_v   = 0.0f;
    uint16_t live_adc_raw     = 0;
    float    live_gross_kg    = 0.0f;
    float    live_net_kg      = 0.0f;
    float    live_hopper_pct  = 0.0f;

    bool     oled_initialized = false;

    void begin() {
        // Configurar ADC1_CH0 (GPIO 1) con atenuación de 11dB (0 - 3.3V)
        analogSetPinAttenuation(PIN_WEIGHT_ADC, ADC_11db);

        // Cargar calibración persistente desde NVS
        loadCalibration();

        // Inicializar lectura inicial
        for (int i = 0; i < 16; i++) {
            updateReadings();
            delay(10);
        }

        // Inicializar pantalla OLED SSD1306 del Heltec V3
        initOled();
    }

    /**
     * -------------------------------------------------------------------------
     * Lectura y Filtrado del ADC (Sobremuestreo y Filtro Pasa-Bajos EMA)
     * -------------------------------------------------------------------------
     */
    void updateReadings() {
        const int SAMPLES = 32;
        uint32_t sum_mv  = 0;
        uint32_t sum_raw = 0;

        for (int i = 0; i < SAMPLES; i++) {
            sum_mv  += analogReadMilliVolts(PIN_WEIGHT_ADC);
            sum_raw += analogRead(PIN_WEIGHT_ADC);
            delayMicroseconds(150);
        }

        float instant_voltage = (sum_mv / (float)SAMPLES) / 1000.0f;
        uint16_t instant_raw  = sum_raw / SAMPLES;

        // Filtro Exponencial Móvil (EMA) para amortiguar vibración mecánica del galpón
        const float alpha = 0.25f;
        if (live_voltage_v <= 0.0001f && live_voltage_v >= -0.0001f) {
            live_voltage_v = instant_voltage;
            live_adc_raw   = instant_raw;
        } else {
            live_voltage_v = (alpha * instant_voltage) + ((1.0f - alpha) * live_voltage_v);
            live_adc_raw   = (uint16_t)((alpha * instant_raw) + ((1.0f - alpha) * live_adc_raw));
        }

        // Cálculo de peso
        float effective_slope = cal_slope_v_per_kg;
        if (fabs(effective_slope) < 0.00001f) {
            effective_slope = 0.0500f; // Evitar división por cero
        }

        live_gross_kg = (live_voltage_v - cal_zero_v) / effective_slope;
        live_net_kg   = live_gross_kg - cal_tare_kg;

        // Calcular porcentaje de llenado de tolva
        if (hopper_capacity_kg > 0.01f) {
            live_hopper_pct = (live_net_kg / hopper_capacity_kg) * 100.0f;
            if (live_hopper_pct < 0.0f) live_hopper_pct = 0.0f;
            if (live_hopper_pct > 100.0f) live_hopper_pct = 100.0f;
        } else {
            live_hopper_pct = 0.0f;
        }
    }

    /**
     * -------------------------------------------------------------------------
     * Calibración Automática: Zero (Tolva vacía)
     * -------------------------------------------------------------------------
     */
    void calibrateZero() {
        // Tomar muestra estabilizada de 64 lecturas
        uint32_t sum_mv = 0;
        for (int i = 0; i < 64; i++) {
            sum_mv += analogReadMilliVolts(PIN_WEIGHT_ADC);
            delay(5);
        }
        cal_zero_v  = (sum_mv / 64.0f) / 1000.0f;
        cal_tare_kg = 0.0f; // Reset de tara
        saveCalibration();
        Serial.printf("[CALIBRATION] Zero fijado en: %.4f V\n", cal_zero_v);
    }

    /**
     * -------------------------------------------------------------------------
     * Calibración Automática: Span (Con peso patrón conocido)
     * -------------------------------------------------------------------------
     */
    bool calibrateSpan(float known_weight_kg) {
        if (known_weight_kg <= 0.01f) {
            Serial.println(F("[CALIBRATION ERROR] El peso para SPAN debe ser mayor a 0 kg"));
            return false;
        }

        // Tomar muestra estabilizada con la carga patrón
        uint32_t sum_mv = 0;
        for (int i = 0; i < 64; i++) {
            sum_mv += analogReadMilliVolts(PIN_WEIGHT_ADC);
            delay(5);
        }
        float span_voltage = (sum_mv / 64.0f) / 1000.0f;
        float delta_v = span_voltage - cal_zero_v;

        if (fabs(delta_v) < 0.005f) {
            Serial.println(F("[CALIBRATION ERROR] Delta de voltaje insuficiente para calibrar Span"));
            return false;
        }

        cal_slope_v_per_kg = delta_v / known_weight_kg;
        saveCalibration();
        Serial.printf("[CALIBRATION] Span calculado: V_span=%.4f V, Delta=%.4f V, Pendiente=%.5f V/kg\n",
                      span_voltage, delta_v, cal_slope_v_per_kg);
        return true;
    }

    /**
     * -------------------------------------------------------------------------
     * Calibración Manual: Pendiente (V/kg) y Cero (V)
     * -------------------------------------------------------------------------
     */
    void setManualSlope(float slope_v_per_kg) {
        if (fabs(slope_v_per_kg) > 0.00001f) {
            cal_slope_v_per_kg = slope_v_per_kg;
            saveCalibration();
            Serial.printf("[CALIBRATION] Pendiente manual asignada: %.5f V/kg\n", cal_slope_v_per_kg);
        }
    }

    void setManualZero(float zero_v) {
        cal_zero_v = zero_v;
        saveCalibration();
        Serial.printf("[CALIBRATION] Cero manual asignado: %.4f V\n", cal_zero_v);
    }

    void setTare() {
        cal_tare_kg = live_gross_kg;
        saveCalibration();
        Serial.printf("[CALIBRATION] Tara aplicada: %.2f kg\n", cal_tare_kg);
    }

    void clearTare() {
        cal_tare_kg = 0.0f;
        saveCalibration();
        Serial.println(F("[CALIBRATION] Tara reseteada a 0.00 kg"));
    }

    void resetDefaults() {
        cal_slope_v_per_kg = DEFAULT_CAL_SLOPE_V_PER_KG;
        cal_zero_v         = DEFAULT_CAL_ZERO_V;
        cal_tare_kg        = 0.0f;
        hopper_capacity_kg = DEFAULT_HOPPER_CAPACITY_KG;
        saveCalibration();
        Serial.println(F("[CALIBRATION] Valores restablecidos a los de fábrica"));
    }

    /**
     * -------------------------------------------------------------------------
     * Persistencia en Flash NVS con Preferences
     * -------------------------------------------------------------------------
     */
    void loadCalibration() {
        preferences.begin("avicola_scale", true);
        cal_slope_v_per_kg = preferences.getFloat("slope", DEFAULT_CAL_SLOPE_V_PER_KG);
        cal_zero_v         = preferences.getFloat("zero", DEFAULT_CAL_ZERO_V);
        cal_tare_kg        = preferences.getFloat("tare", 0.0f);
        hopper_capacity_kg = preferences.getFloat("cap", DEFAULT_HOPPER_CAPACITY_KG);
        preferences.end();
        Serial.printf("[NVS LOAD] Calibración: Pendiente=%.5f V/kg, Cero=%.4f V, Tara=%.2f kg, Cap=%.1f kg\n",
                      cal_slope_v_per_kg, cal_zero_v, cal_tare_kg, hopper_capacity_kg);
    }

    void saveCalibration() {
        preferences.begin("avicola_scale", false);
        preferences.putFloat("slope", cal_slope_v_per_kg);
        preferences.putFloat("zero", cal_zero_v);
        preferences.putFloat("tare", cal_tare_kg);
        preferences.putFloat("cap", hopper_capacity_kg);
        preferences.end();
    }

    /**
     * -------------------------------------------------------------------------
     * Pantalla OLED Heltec V3 (128x64 SSD1306)
     * -------------------------------------------------------------------------
     * Cumple con: "ocupa toda la pantalla (es de un heltec v3) y que indique
     * la cantidad de comida en un recuadro".
     */
    void initOled() {
        // En Heltec V3, Vext (GPIO 36) ya debe estar en LOW para alimentar la pantalla
        pinMode(PIN_OLED_RST, OUTPUT);
        digitalWrite(PIN_OLED_RST, LOW);
        delay(30);
        digitalWrite(PIN_OLED_RST, HIGH);
        delay(50);

        Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);

        if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
            Serial.println(F("[OLED] Warning: No se pudo inicializar SSD1306 en 0x3C"));
            oled_initialized = false;
            return;
        }

        oled_initialized = true;
        display.clearDisplay();
        display.setTextColor(SSD1306_WHITE);

        // Pantalla de bienvenida
        display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
        display.setTextSize(1);
        display.setCursor(8, 8);
        display.println(F("BALANZA AVICOLA"));
        display.setCursor(8, 22);
        display.println(F("Heltec LoRa32 V3"));
        display.setCursor(8, 38);
        display.println(F("Iniciando Sistema..."));
        display.setCursor(8, 50);
        display.println(F("ADC0 / SX1262 / WiFi"));
        display.display();
    }

    void updateOled(float toa_ms, const char* lora_status, bool tx_in_progress) {
        if (!oled_initialized) return;

        display.clearDisplay();

        // 1. Marco exterior que abarca toda la pantalla de 128x64
        display.drawRect(0, 0, 128, 64, SSD1306_WHITE);

        // 2. Barra de Título Superior
        display.fillRect(0, 0, 128, 12, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
        display.setTextSize(1);
        display.setCursor(4, 2);
        display.print(F("BALANZA GALPON [WiFi]"));

        // 3. Recuadro Central Destacado de Cantidad de Comida (ocupa el centro)
        display.drawRect(3, 14, 122, 34, SSD1306_WHITE);
        display.setTextColor(SSD1306_WHITE);
        display.setTextSize(1);
        display.setCursor(7, 17);
        display.print(F("COMIDA:"));

        // Cantidad de comida en tipografía grande (Tamaño 2)
        display.setTextSize(2);
        display.setCursor(12, 28);
        display.printf("%6.2f kg", live_net_kg);

        // 4. Barra Inferior de Métricas (Voltaje y ToA)
        display.setTextSize(1);
        display.setCursor(4, 51);
        if (tx_in_progress) {
            display.print(F("TX LORA... ESPERE"));
        } else {
            display.printf("%.2fV | ToA:%dms", live_voltage_v, (int)toa_ms);
        }

        display.display();
    }

private:
    Adafruit_SSD1306 display;
    Preferences      preferences;
};

#endif // SCALE_DRIVER_H
