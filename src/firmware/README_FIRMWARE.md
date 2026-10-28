# Guía de Conexión y Compilación del Firmware ESP32 + Ra-02 (Fase 1)

## 1. Diagrama de Conexiones de Hardware

El módulo Ai-Thinker **Ra-02** (transceptor Semtech SX1278 / SX1276) se comunica con el microcontrolador ESP32 a través del bus SPI y líneas de interrupción GPIO dedicadas.

| Pin Ra-02 (SX1278) | Pin ESP32 DevKit | Función SPI / LoRaWAN | Notas |
| :--- | :--- | :--- | :--- |
| **VCC** (3.3V) | **3V3** | Alimentación | **NUNCA conectar a 5V**, el chip opera a 3.3V estrictos. |
| **GND** | **GND** | Tierra común | Conexión a plano de tierra. |
| **SCK** | **GPIO 18** | SPI Clock | Bus SPI estándar (VSPI). |
| **MISO** | **GPIO 19** | SPI Master In Slave Out | Bus SPI estándar. |
| **MOSI** | **GPIO 23** | SPI Master Out Slave In | Bus SPI estándar. |
| **NSS / CS** | **GPIO 5** | SPI Chip Select | Control de bus SPI (configurable en `config.h`). |
| **RST** | **GPIO 14** | Hardware Reset | Reset del transceptor LoRa. |
| **DIO0** | **GPIO 26** | Interrupción TX/RX Done | **OBLIGATORIO** para saber cuándo finaliza TX/RX. |
| **DIO1** | **GPIO 33** | Interrupción RX Timeout | **OBLIGATORIO** para sincronizar ventanas RX1 y RX2. |
| **DIO2** | *No conectado* | Interrupción FSK/FHSS | Opcional (definido como `LMIC_UNUSED_PIN`). |

> [!IMPORTANT]
> Los pines **DIO0** y **DIO1** son ambos estrictamente obligatorios en LoRaWAN Class A para poder capturar la ventana de recepción descendente (*Downlink*) tras enviar telemetría.

---

## 2. Configuración de Credenciales OTAA en `include/config.h`

En ChirpStack v4, al crear el dispositivo obtendrás `DevEUI`, `AppEUI` (o JoinEUI) y `AppKey`.

Ten en cuenta el orden de bytes (*Endianness*):
* **APPEUI**: 8 bytes en formato **Little-Endian** (LSB primero).
* **DEVEUI**: 8 bytes en formato **Little-Endian** (LSB primero). Si ChirpStack muestra `01 02 03 04 05 06 07 08`, en el código C++ se escribe `{0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01}`.
* **APPKEY**: 16 bytes en formato **Big-Endian** (MSB primero, tal como lo muestra ChirpStack).

---

## 3. Compilación y Carga

### Opción A: PlatformIO (Recomendado)
```bash
cd src/firmware

# Compilar para banda US915 (Laird RG1 915 MHz)
pio run -e esp32_us915

# Cargar al ESP32 conectado por USB
pio run -e esp32_us915 --target upload

# Abrir monitor serie a 115200 baudios
pio device monitor -b 115200
```

### Opción B: Arduino IDE
1. Instala la biblioteca `MCCI LoRaWAN LMIC library` desde el Library Manager.
2. Abre el archivo de configuración de LMIC `libraries/MCCI_LoRaWAN_LMIC_library/project_config/lmic_project_config.h` y habilita la banda correcta:
   ```c
   #define CFG_us915 1
   #define CFG_sx1276_radio 1
   ```
3. Copia `main.cpp`, `sdn_header.h` y `config.h` en una misma carpeta de sketch (ej. `sdn_node`).
4. Selecciona la placa **ESP32 Dev Module** y el puerto COM/ttyUSB correspondiente.
5. Carga y abre el Serial Monitor a **115200 baudios**.

---

## 4. Salida Esperada en el Monitor Serial

### Durante el Inicio y Envío de Telemetría (Uplink 204 Bytes):
```text
=======================================================
   ESP32 + Ra-02 (SX1276/78) SDN-LoRa Node Starting   
   Phase 1: Pipeline & Bi-directional Communication    
=======================================================
SdnHeader struct size: 4 bytes (Verified)
[RF] Region: US915 configured.
[LMIC Time: 120] Event: EV_JOINING: Starting LoRaWAN OTAA Join request...
[LMIC Time: 840] Event: EV_JOINED: Successfully joined network!

[TX] Uplink Queued: #1 | Size: 204 Bytes | Int: 60s | SF7 @ 14 dBm
[LMIC Time: 1540] Event: EV_TXCOMPLETE (Transmission and RX windows closed)
[SCHEDULER] Next transmission in 60 seconds.
```

### Al Recibir una Regla SDN Descendente (Downlink 4 Bytes):
```text
[LMIC Time: 5240] Event: EV_TXCOMPLETE (Transmission and RX windows closed)
  -> Downlink payload received! Length: 4 bytes | Port: 10

=======================================================
📥 [SDN] DOWNLINK RECEIVED IN RX WINDOW
=======================================================
  Raw Data:       0x00000F01 (Bytes: 0x01 0x0F 0x00 0x00)
  Payload Length: 4 bytes (SDN Header: 4 bytes)
-------------------------------------------------------
  ├─ cmd_id:       0x01 (RECONFIG_RADIO)
  ├─ sf_target:    0 -> SF7
  ├─ tx_power:     14 dBm
  ├─ sample_rate:  1 -> 60 seconds
  ├─ payload_mode: 0 -> FULL_TELEMETRY (204 Bytes)
  ├─ sleep_enable: 0 (DISABLED)
  └─ param_value:  0 (0x0000)
-------------------------------------------------------
[SDN ACTION] RECONFIG_RADIO applied:
             - Spreading Factor: SF7 -> SF7
             - Tx Power:         14 dBm -> 14 dBm
             - Uplink Interval:  60 s -> 60 s
             - Payload Mode:     0 -> FULL_TELEMETRY (204 Bytes)
=======================================================
[SCHEDULER] Next transmission in 60 seconds.
```
