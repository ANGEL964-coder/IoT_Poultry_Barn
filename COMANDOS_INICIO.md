# Guía Rápida de Comandos para Iniciar la Red SDN-LoRa (Fase 1)

---

## 1. Datos Clave de Configuración

| Parámetro | Valor a Configurar | Descripción |
| :--- | :--- | :--- |
| **IP de la Raspberry Pi** | **`10.227.26.23`** | IP local detectada en tu red LAN / Wi-Fi |
| **IP a poner en el Gateway Laird RG1** | **`10.227.26.23`** | Campo *Server Address* en Semtech Packet Forwarder |
| **Puertos UDP Gateway** | **`1700`** (Upstream y Downstream) | Puerto donde escucha `chirpstack-gateway-bridge` |
| **Consola Web ChirpStack v4** | [http://10.227.26.23:8080](http://10.227.26.23:8080) | Usuario/Contraseña por defecto: `admin` / `admin` |
| **Broker MQTT Mosquitto** | `10.227.26.23:1883` | Conexión local para el controlador SDN |
| **Base de Datos Redis** | `10.227.26.23:6379` | Tabla Global de Estado de Nodos |

---

## 2. Configuración en la Interfaz Web del Gateway (Laird Sentrius RG1)

1. Abre tu navegador y accede a la IP del gateway: `http://<IP_DEL_LAIRD_RG1>`.
2. Ve al menú **LoRa** -> **Forwarder**.
3. Selecciona **Semtech Packet Forwarder**.
4. Configura los siguientes campos:
   * **Server Address:** `10.227.26.23`
   * **Upstream Port:** `1700`
   * **Downstream Port:** `1700`
5. Haz clic en **Save Changes** y luego en **Restart Forwarder**.

---

## 3. Estado de la Infraestructura en la Raspberry Pi (Docker)

Docker ya está instalado y los 5 contenedores se encuentran **activos y corriendo**:
- `sdn_gateway_bridge` (UDP 1700)
- `sdn_chirpstack` (TCP 8080)
- `sdn_mosquitto` (TCP 1883)
- `sdn_redis` (TCP 6379)
- `sdn_postgres` (TCP 5432)

### Comandos de gestión de Docker:
```bash
# Ver el estado de los contenedores
sudo docker ps

# Ver logs de ChirpStack
sudo docker logs -f sdn_chirpstack

# Ver logs del Gateway Bridge (paquetes UDP del Laird RG1)
sudo docker logs -f sdn_gateway_bridge

# Reiniciar toda la pila si es necesario
cd /home/server/CONTROLADOR_SDN
sudo docker compose restart
```

---

## 4. Iniciar el Componente de Borde (Pipeline MQTT -> Redis)

En una terminal en la Raspberry Pi:
```bash
cd /home/server/CONTROLADOR_SDN
source .venv/bin/activate
python3 src/edge/test_pipeline.py
```
> Esta terminal quedará a la escucha. Al recibir un paquete de 204 bytes del Heltec V3, extraerá DevEUI, RSSI, SNR y guardará atómicamente el estado en Redis.

---

## 5. Inyectar Reglas de Control SDN hacia el Heltec V3 (Downlink)

En **otra terminal** en la Raspberry Pi:
```bash
cd /home/server/CONTROLADOR_SDN
source .venv/bin/activate

# Reconfigurar nodo a SF7, 14 dBm, intervalo 60s:
python3 src/edge/send_test_downlink.py \
  --app-id <CHIRPSTACK_APP_ID> \
  --dev-eui 0102030405060708 \
  --cmd-id 1 \
  --sf-target 0 \
  --tx-power 14 \
  --sample-rate 1

# Enviar comando Keep-Alive (0x00):
python3 src/edge/send_test_downlink.py \
  --app-id <CHIRPSTACK_APP_ID> \
  --dev-eui 0102030405060708 \
  --cmd-id 0

# Ajustar umbral de temperatura a 26.5 °C (param_value = 2650):
python3 src/edge/send_test_downlink.py \
  --app-id <CHIRPSTACK_APP_ID> \
  --dev-eui 0102030405060708 \
  --cmd-id 2 \
  --param-value 2650
```

---

## 6. Consultar el Estado del Nodo en Redis

Para verificar en tiempo real los datos almacenados en memoria:
```bash
# Ver todos los campos del nodo
redis-cli HGETALL nodo:0102030405060708

# Ver el historial de los últimos uplinks
redis-cli LRANGE nodo:0102030405060708:history 0 5
```

---

## 7. Compilación y Carga en el HELTEC WiFi LoRa 32 V3

El código para el **Heltec V3 (ESP32-S3 + SX1262)** se encuentra en `src/firmware/heltec_v3/`.

### Características implementadas:
- Control de alimentación **`Vext` en GPIO 36** (activo en LOW) para energizar el transceptor SX1262.
- Conexión SPI nativa del Heltec V3 (NSS=8, SCK=9, MOSI=10, MISO=11, RST=12, BUSY=13, DIO1=14).
- Pila LoRaWAN moderna con **RadioLib**.
- Envío de telemetría periódica de **204 bytes**.
- Recepción de la cabecera SDN de **32 bits (4 bytes)** y ejecución de máquina de estados.

### Comandos de carga (con PlatformIO):
```bash
cd /home/server/CONTROLADOR_SDN/src/firmware/heltec_v3

# Compilar para banda US915
pio run -e heltec_v3_us915

# Subir por cable USB al Heltec V3
pio run -e heltec_v3_us915 --target upload

# Abrir monitor serie a 115200 baudios
pio device monitor -b 115200
```
