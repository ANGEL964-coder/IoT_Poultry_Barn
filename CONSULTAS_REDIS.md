# Guía de Comandos para Consultar la Tabla Global de Estado en Redis (SDN-LoRa)

Esta guía contiene los comandos fundamentales para auditar, monitorear y consultar el estado de los nodos LoRaWAN almacenados en la base de datos **Redis** por el pipeline de borde (`src/edge/test_pipeline.py`).

---

## 1. Métodos de Acceso a Redis

Dado que Redis se ejecuta dentro de un contenedor Docker (`sdn_redis`), tienes dos formas de ejecutar comandos:

### Método A: Directo mediante Docker (Recomendado - Sin instalar nada)
```bash
# Modo interactivo (consola de Redis CLI):
docker exec -it sdn_redis redis-cli

# Ejecutar un comando directo desde la terminal Bash:
docker exec sdn_redis redis-cli <COMANDO>
```

### Método B: Si instalas el cliente nativo en la Raspberry Pi
```bash
sudo apt update && sudo apt install -y redis-tools
redis-cli <COMANDO>
```

---

## 2. Estructura de Datos Almacenada en Redis

Cuando el pipeline recibe una trama LoRaWAN de 204 bytes, guarda dos estructuras por cada nodo:

1. **Hash de Estado Actual (`nodo:<DEV_EUI>`):** Contiene las últimas métricas y telemetría reportadas por el nodo.
2. **Lista Histórica (`nodo:<DEV_EUI>:history`):** Almacena en formato JSON los últimos 50 eventos de telemetría (orden cronológico inverso, el más reciente está en el índice `0`).

### Campos del Hash `nodo:<DEV_EUI>`:
| Campo | Tipo | Descripción | Ejemplo |
| :--- | :--- | :--- | :--- |
| `dev_eui` | String | DevEUI único del dispositivo LoRa | `70b3d57ed007985a` |
| `rssi` | Float | Calidad de señal recibida (dBm) | `-82.0` |
| `snr` | Float | Relación señal-ruido (dB) | `9.5` |
| `gateway_id` | Hex | EUI del Gateway que recibió la trama | `c0ee40ffff29424b` |
| `fcnt` | Int | Contador de tramas LoRaWAN (Uplink Frame Counter) | `42` |
| `fport` | Int | Puerto LoRaWAN de la aplicación | `10` |
| `payload_len` | Int | Tamaño del payload útil en bytes | `204` |
| `payload_hex` | Hex | Payload completo decodificado en Hexadecimal | `010A...` |
| `payload_b64` | String | Payload en Base64 tal como llegó de ChirpStack | `AQr...` |
| `last_seen_iso` | String | Fecha y hora UTC (ISO-8601) de la última recepción | `2026-11-04T15:45:00.123456+00:00` |
| `last_seen_epoch`| Int | Timestamp UNIX en segundos | `1793807100` |

---

## 3. Comandos Esenciales de Consulta (Directos desde Bash)

Reemplaza `70b3d57ed007985a` por el DevEUI de tu nodo si utilizas otro.

### 3.1 Listar todos los nodos registrados
```bash
# Ver todas las claves asociadas al pipeline SDN:
docker exec sdn_redis redis-cli KEYS "nodo:*"

# Ver solo las claves de estado (excluyendo historiales):
docker exec sdn_redis redis-cli KEYS "nodo:????????????????"
```

### 3.2 Consultar todo el estado actual de un nodo
Muestra todos los campos del nodo y sus valores:
```bash
docker exec sdn_redis redis-cli HGETALL nodo:70b3d57ed007985a
```

### 3.3 Consultar métricas de radiofrecuencia (RSSI, SNR y última hora visto)
Obtiene únicamente la calidad de señal y la marca de tiempo:
```bash
docker exec sdn_redis redis-cli HMGET nodo:70b3d57ed007985a rssi snr last_seen_iso
```

### 3.4 Consultar el contador de tramas y tamaño de payload
Verifica que las tramas sigan incrementando y lleguen con los 204 bytes requeridos:
```bash
docker exec sdn_redis redis-cli HMGET nodo:70b3d57ed007985a fcnt payload_len
```

### 3.5 Ver el volcado hexadecimal del último paquete recibido
```bash
docker exec sdn_redis redis-cli HGET nodo:70b3d57ed007985a payload_hex
```

### 3.6 Comprobar si un nodo existe en la base de datos
Retorna `1` si existe o `0` si aún no ha reportado:
```bash
docker exec sdn_redis redis-cli EXISTS nodo:70b3d57ed007985a
```

---

## 4. Consultas al Historial de Paquetes (`history`)

Cada recepción se apila en una lista circular (`LPUSH` limitado a los 50 últimos registros).

### 4.1 Ver el último paquete registrado (el más reciente)
```bash
docker exec sdn_redis redis-cli LINDEX nodo:70b3d57ed007985a:history 0
```

### 4.2 Ver los últimos 5 paquetes recibidos
```bash
docker exec sdn_redis redis-cli LRANGE nodo:70b3d57ed007985a:history 0 4
```

### 4.3 Contar cuántos paquetes históricos hay almacenados
```bash
docker exec sdn_redis redis-cli LLEN nodo:70b3d57ed007985a:history
```

---

## 5. Monitoreo en Tiempo Real (Depuración en Vivo)

Para observar en vivo cada lectura o escritura que se hace sobre Redis a medida que llegan los paquetes LoRaWAN:
```bash
docker exec -it sdn_redis redis-cli MONITOR
```
*(Presiona `Ctrl + C` para salir del monitor).*

---

## 6. Comandos de Limpieza para Nuevas Pruebas

Si deseas reiniciar los contadores o limpiar la base de datos antes de una nueva prueba de comunicación:

### Borrar los datos de un solo nodo:
```bash
docker exec sdn_redis redis-cli DEL nodo:70b3d57ed007985a nodo:70b3d57ed007985a:history
```

### Borrar todos los nodos SDN preservando la base de datos:
```bash
docker exec sdn_redis redis-cli --eval "return redis.call('del', unpack(redis.call('keys', 'nodo:*')))"
```

### Vaciar completamente la base de datos Redis:
> [!WARNING]
> Esto eliminará también los streams temporales y cachés de ChirpStack. ChirpStack los regenerará automáticamente con nuevo tráfico.
```bash
docker exec sdn_redis redis-cli FLUSHDB
```

---

## 7. Script Rápido en Python para Ver el Estado Formateado

Si prefieres una vista en consola bien organizada, puedes ejecutar directamente:
```bash
source .venv/bin/activate
python3 -c "
import redis, json
r = redis.Redis()
for key in r.keys('nodo:[0-9a-fA-F]*'):
    if b':history' not in key:
        data = r.hgetall(key)
        print('='*50)
        print('NODO:', key.decode())
        for k, v in data.items():
            print(f'  {k.decode():<15}: {v.decode()}')
"
```
