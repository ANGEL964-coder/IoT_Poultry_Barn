# Arquitectura SDN sobre LoRa (Fase 1: Validación de Pipeline y Comunicación)

Este repositorio contiene la implementación de la **Fase 1** de la arquitectura SDN adaptada sobre LoRaWAN (conforme al marco **RFC 7426**), garantizando la validación de la comunicación bidireccional de extremo a extremo (*Uplink* de telemetría y *Downlink* de cabecera de control SDN de 4 bytes) entre los nodos finales (ESP32 + Ra-02), el Gateway (Laird Sentrius RG1), el Network Server local (ChirpStack v4 + Mosquitto + Redis) y el Componente de Borde en Python.

---

## 1. Estructura del Repositorio

```text
CONTROLADOR_SDN/
├── config/
│   ├── chirpstack/
│   │   └── chirpstack.toml                  # Configuración principal de ChirpStack v4
│   ├── chirpstack-gateway-bridge/
│   │   └── chirpstack-gateway-bridge.toml   # Listener UDP 1700 para Laird Sentrius RG1
│   └── mosquitto/
│       └── mosquitto.conf                   # Configuración del broker MQTT local (1883)
├── docker-compose.yml                       # Orquestación de ChirpStack v4, Mosquitto, Redis y Postgres
├── requirements.txt                         # Dependencias Python (paho-mqtt, redis, pytest)
├── src/
│   ├── edge/
│   │   ├── __init__.py
│   │   ├── sdn_protocol.py                  # Codec binario y empaquetador de la cabecera SDN de 32 bits
│   │   ├── test_pipeline.py                 # Listener MQTT -> Extracción RSSI/SNR/Payload -> Escritura atómica en Redis
│   │   └── send_test_downlink.py            # CLI para inyectar paquetes de bajada en Base64 vía MQTT
│   └── firmware/
│       ├── platformio.ini                   # Proyecto PlatformIO para ESP32 (LMIC LoRaWAN)
│       ├── include/
│       │   ├── sdn_header.h                 # Definición de struct bit-fields packed de 32 bits
│       │   └── config.h                     # Mapeo de pines Ra-02 y credenciales OTAA
│       ├── src/
│       │   └── main.cpp                     # Firmware ESP32: Telemetría 204B + Deserializador Downlink
│       └── README_FIRMWARE.md               # Guía de conexionado y flasheo de hardware
├── tests/
│   ├── __init__.py
│   ├── test_sdn_codec.py                    # Pruebas unitarias de empaquetado/desempaquetado de bits
│   └── mock_pipeline_e2e.py                 # Prueba de integración de extremo a extremo simulada
└── README.md                                # Documentación general y Smoke Test
```

---

## 2. Especificación de la Cabecera de Control SDN (32 Bits / 4 Bytes)

La cabecera de bajada se transporta en la ventana RX de LoRaWAN y se mapea de forma bit a bit (`struct __attribute__((packed)) SdnHeader`):

| Campo | Rango de Bits | Ancho | Valores | Descripción Técnica |
| :--- | :---: | :---: | :---: | :--- |
| **`cmd_id`** | Bit 0 – 3 | 4 bits | `0x0` a `0xF` | Código de instrucción (`0x00`=NOP, `0x01`=RECONFIG_RADIO, `0x02`=TEMP, `0x03`=SLEEP, `0x0F`=RESET) |
| **`sf_target`** | Bit 4 – 6 | 3 bits | `0` a `7` | Índice Spreading Factor (`0` $\rightarrow$ SF7, `1` $\rightarrow$ SF8, `2` $\rightarrow$ SF9, `3` $\rightarrow$ SF10, `4` $\rightarrow$ SF11, `5` $\rightarrow$ SF12) |
| **`tx_power`** | Bit 7 – 10 | 4 bits | `0` a `15` | Potencia de transmisión en dBm inyectada al amplificador del módulo LoRa |
| **`sample_rate`** | Bit 11 – 13 | 3 bits | `0` a `7` | Periodo de telemetría (`0`=10s, `1`=60s, `2`=300s, `3`=900s, `4`=1800s, `5`=3600s, `6`=14400s, `7`=86400s) |
| **`payload_mode`** | Bit 14 – 15 | 2 bits | `0` a `3` | Modo de carga útil (`0`=Full Telemetry 204B, `1`=Critical Alarm, `2`=Diagnostic) |
| **`sleep_enable`** | Bit 16 | 1 bit | `0` o `1` | Bandera de activación de Deep Sleep (`0`=Off, `1`=Activo con temporizador RTC) |
| **`param_value`** | Bit 17 – 31 | 15 bits | `0` a `32,767` | Argumento multipropósito de la regla (ej. umbral de temperatura, minutos de sueño, ID de nodo) |

---

## 3. Despliegue de Infraestructura en la Raspberry Pi

### 3.1 Instalar Docker y Docker Compose (si no están instalados)
```bash
curl -fsSL https://get.docker.com -o get-docker.sh
sudo sh get-docker.sh
sudo usermod -aG docker $USER
newgrp docker
```

### 3.2 Levantar la Pila (ChirpStack v4, Gateway Bridge, Mosquitto, Redis, Postgres)
```bash
cd /home/server/CONTROLADOR_SDN
docker compose up -d
```

Verificar que todos los servicios estén en estado `running` y los puertos expuestos:
- **UDP 1700**: Escuchando paquetes Semtech Packet Forwarder del Laird RG1.
- **TCP 1883**: Mosquitto MQTT Broker.
- **TCP 6379**: Redis In-Memory State Table.
- **TCP 8080**: Consola Web de ChirpStack (`http://<IP_RASPBERRY_PI>:8080`).

### 3.3 Configuración del Gateway Laird Sentrius RG1
1. Accede a la interfaz web del Laird RG1 (`http://<IP_GATEWAY>`).
2. En la sección **LoRa** -> **Forwarder**, selecciona **Semtech Semtech Packet Forwarder**.
3. Configura:
   - **Server Address:** IP local de la Raspberry Pi (ej. `192.168.1.100`).
   - **Upstream Port:** `1700`.
   - **Downstream Port:** `1700`.
4. Guarda y reinicia el forwarder en el gateway.

---

## 4. Configuración del Componente de Borde (Python)

### 4.1 Entorno Virtual y Dependencias
```bash
cd /home/server/CONTROLADOR_SDN
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

### 4.2 Iniciar el Validador de Pipeline (Uplink -> Redis)
```bash
python3 src/edge/test_pipeline.py
```
El script se suscribe a `application/+/device/+/event/up`. Al llegar un paquete de telemetría de 204 bytes desde el ESP32:
- Extrae `devEui`, `rssi`, `snr`, `fCnt`, `fPort` y desencripta el payload.
- Almacena atómicamente el estado en el Hash de Redis con la clave `nodo:<DEV_EUI>`.
- Registra el evento en la lista histórica `nodo:<DEV_EUI>:history`.

### 4.3 Inyectar Regla SDN de Downlink (CLI)
Para enviar una instrucción de control SDN al nodo en su ventana de recepción RX:
```bash
# Reconfigurar nodo a SF7, 14 dBm, intervalo de 60 segundos:
python3 src/edge/send_test_downlink.py \
  --app-id <CHIRPSTACK_APP_ID> \
  --dev-eui <DEV_EUI> \
  --cmd-id 1 \
  --sf-target 0 \
  --tx-power 14 \
  --sample-rate 1

# Enviar comando Keep-Alive (NOP):
python3 src/edge/send_test_downlink.py \
  --app-id <CHIRPSTACK_APP_ID> \
  --dev-eui <DEV_EUI> \
  --cmd-id 0

# Ajustar umbral de temperatura a 28.5 °C (param_value = 2850):
python3 src/edge/send_test_downlink.py \
  --app-id <CHIRPSTACK_APP_ID> \
  --dev-eui <DEV_EUI> \
  --cmd-id 2 \
  --param-value 2850
```

---

## 5. Preparación del Firmware en el ESP32

1. Conecta el módulo Ra-02 al ESP32 según la tabla de pines en [`src/firmware/README_FIRMWARE.md`](file:///home/server/CONTROLADOR_SDN/src/firmware/README_FIRMWARE.md).
2. Configura las llaves OTAA (`APPEUI`, `DEVEUI`, `APPKEY`) en [`src/firmware/include/config.h`](file:///home/server/CONTROLADOR_SDN/src/firmware/include/config.h).
3. Compila y carga usando PlatformIO:
   ```bash
   cd src/firmware
   pio run -e esp32_us915 --target upload
   pio device monitor -b 115200
   ```

---

## 6. Procedimiento de Pruebas de Humo (Smoke Test) Paso a Paso

Para validar la comunicación de extremo a extremo sin pérdidas:

### Paso 1: Ejecutar Pruebas de Software Automatizadas
Valida el codec de bits y el flujo completo simulado:
```bash
source .venv/bin/activate
PYTHONPATH=. pytest tests/test_sdn_codec.py
python3 tests/mock_pipeline_e2e.py
```
> Resultado esperado: 5 pruebas unitarias aprobadas al 100% y simulación E2E exitosa confirmando detección de los 204 bytes.

### Paso 2: Arrancar el Pipeline de Borde
En la Raspberry Pi, ejecuta en una terminal:
```bash
python3 src/edge/test_pipeline.py
```

### Paso 3: Encender el ESP32
Observa el monitor serie del microcontrolador. Debe verse:
1. `EV_JOINING` seguido de `EV_JOINED`.
2. Emisión del paquete `#1`: `[TX] Uplink Queued: #1 | Size: 204 Bytes | Int: 60s | SF7 @ 14 dBm`.

### Paso 4: Confirmar la Recepción en la Raspberry Pi
En la consola de `test_pipeline.py` debe aparecer inmediatamente el bloque de diagnóstico:
```text
=================================================================
📡 [UPLINK EVENT #1] Node: 0102030405060708
   ├─ Timestamp:    2026-10-28T15:10:00+00:00
   ├─ Frame Cnt:    fCnt=1 (Port: 10)
   ├─ RF Quality:   RSSI=-68.0 dBm | SNR=9.5 dB (Gateway: 0016c001f1123456)
   ├─ Payload Size: [204 Bytes MATCH]
   ├─ Hex Dump:     01000000E803000000...
   └─ Redis Hash:   HSET 'nodo:0102030405060708' (Atomic Commit OK)
=================================================================
```

Comprobar el Hash almacenado en Redis:
```bash
redis-cli HGETALL nodo:0102030405060708
```

### Paso 5: Enviar Regla de Bajada (Downlink)
En otra terminal en la Raspberry Pi, ejecuta:
```bash
python3 src/edge/send_test_downlink.py \
  --app-id <CHIRPSTACK_APP_ID> \
  --dev-eui 0102030405060708 \
  --cmd-id 1 \
  --sf-target 1 \
  --tx-power 14 \
  --sample-rate 2 \
  --payload-mode 1
```

### Paso 6: Verificar Deserialización en el ESP32
En el siguiente ciclo de transmisión del ESP32, el Gateway transmitirá la trama en la ventana RX1/RX2. En el monitor serie del ESP32 se observará:
```text
📥 [SDN] DOWNLINK RECEIVED IN RX WINDOW
  ├─ cmd_id:       0x01 (RECONFIG_RADIO)
  ├─ sf_target:    1 -> SF8
  ├─ tx_power:     14 dBm
  ├─ sample_rate:  2 -> 300 seconds
  ├─ payload_mode: 1 -> CRITICAL_ALARM
  ├─ sleep_enable: 0 (DISABLED)
  └─ param_value:  0 (0x0000)
-------------------------------------------------------
[SDN ACTION] RECONFIG_RADIO applied:
             - Spreading Factor: SF7 -> SF8
             - Tx Power:         14 dBm -> 14 dBm
             - Uplink Interval:  60 s -> 300 s
             - Payload Mode:     0 -> CRITICAL_ALARM
```

Con este paso queda validado el ciclo completo bidireccional de la **Fase 1**.
