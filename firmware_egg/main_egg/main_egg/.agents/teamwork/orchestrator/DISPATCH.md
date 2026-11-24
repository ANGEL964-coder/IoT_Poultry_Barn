## 2026-11-15T22:25:25Z

Optimizar el firmware para Heltec WiFi LoRa 32 V3 corrigiendo la suspension inmediata en modo deep sleep, refinando la interfaz OLED del menu principal, integrando reloj RTC interno, implementando modo automatico de ahorro de bateria con muestreo configurable de ADC, y entregando una auditoria energetica antes/despues con recomendaciones de hardware y software, manteniendo intacta la version 1.0 en main.

Working directory: c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot
Integrity mode: development

## Requirements

### R1. Correccion del Modo de Suspension (Deep Sleep)
Resolver la falla de despertar inmediato al entrar en suspension (causada por mantener presionado el boton PRG en GPIO 0 al activarse la ext0 wakeup source en nivel LOW). El sistema debe esperar la liberacion completa del boton (con debounce) antes de ejecutar `esp_deep_sleep_start()`, garantizando que el ESP32 permanezca suspendido hasta una pulsacion intencional posterior.

### R2. Rediseño del Menu Principal en Pantalla OLED
Refinar visualmente la interfaz grafica en la pantalla OLED SSD1306 (128x64 I2C). Eliminar el diseño tosco ("bruto") actual y retirar la leyenda o texto que indica mantener presionado PRG para menu. La interfaz debe ser limpia, tecnica, legible y presentar estrictamente cero emojis.

### R3. Soporte de Reloj RTC Interno
Integrar la funcionalidad de reloj de tiempo real utilizando el RTC interno del ESP32-S3. La hora del sistema debe mantenerse durante el funcionamiento y admitir sincronizacion de red LoRaWAN (DeviceTimeReq) cuando la infraestructura de red este disponible, sin requerir modulos de hardware externos adicionales.

### R4. Modo de Ahorro de Bateria Automatico e Intervalo de ADC Configurable
Implementar un modo de ahorro de bateria automatico en el cual las lecturas de los canales ADC (celda de carga en GPIO 2 y medicion de bateria en GPIO 1 a traves del MOSFET de control en GPIO 37) se realicen unicamente al momento de transmitir el paquete o segun un intervalo programable. Incorporar una opcion en el menu OLED para configurar la frecuencia o intervalo de muestreo del ADC para optimizar el consumo de energia.

### R5. Auditoria de Eficiencia y Ahorro Energetico (Hardware y Software)
Generar un informe tecnico en formato Markdown (`AUDITORIA_ENERGIA.md`) que compare de forma cuantitativa el consumo de corriente (estimado y medido en mA/uA) antes y despues de las optimizaciones para los estados activo, medicion, transmision LoRa y deep sleep. El informe debe documentar recomendaciones especificas de hardware (control de riel Vext en GPIO 36, conmutacion del divisor de bateria en GPIO 37, sleep de perifericos SX1262 y display OLED) y software.

### R6. Alcance de Archivos y Restricciones
Aplicar las modificaciones en los dos entornos de desarrollo: `heltec_v3_local_battery\heltec_v3_local_battery.ino` y `heltec_v3_local_diag\heltec_v3_local_diag.ino`. Mantener estrictamente intacta la version 1.0 ubicada en `main\`. Prohibicion absoluta de emojis en todo el codigo fuente, cadenas de texto, interfaz OLED y documentacion tecnica.
