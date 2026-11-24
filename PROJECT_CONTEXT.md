# Contexto rápido del proyecto

Lee este archivo al iniciar una tarea. Después abre únicamente los archivos del área afectada; evita cargar toda la documentación en cada conversación.

## Objetivo

Sistema de monitoreo avícola y control SDN sobre LoRaWAN. Los nodos Heltec/ESP32 envían telemetría por LoRaWAN; ChirpStack y Mosquitto reciben los eventos; el backend Java persiste y presenta datos en un dashboard y permite emitir comandos SDN.

## Arquitectura actual

`Heltec V3 / firmware` → `Gateway Laird RG1` → `ChirpStack v4` → `MQTT Mosquitto` → `dashboard-java (Spring Boot)` → `PostgreSQL / dashboard web`.

El despliegue previsto tiene **3 nodos LoRaWAN**: contador de huevos, sensor ambiental y balanza de peso. Comparten el gateway Laird RG1; cada nodo debe tener un DevEUI propio. El repositorio también conserva un pipeline Python MQTT → Redis y un firmware ESP32/RA-02 anterior. No asumir que esos componentes son el flujo principal actual: confirma el punto de entrada y la tarea concreta antes de modificarlos.

## Mapa de código

- `dashboard-java/`: backend Spring Boot, frontend estático, seguridad, persistencia, MQTT, WebSocket y API.
- `dashboard-java/src/main/java/com/sdn/poultry/protocol/`: codec del encabezado SDN y decodificador de telemetría.
- `src/firmware/heltec_v3/`: firmware actual para Heltec V3/SX1262; `src/firmware/` también contiene una variante ESP32/RA-02.
- `src/edge/`: pipeline Python heredado y utilidades Redis/MQTT.
- `firmware_egg/` y `user_firmware/`: variantes de firmware relacionadas con el contador de huevos y el nodo de campo; confirma cuál se usa para cada dispositivo antes de editar.
- `config/` y `docker-compose.yml`: configuración de ChirpStack, Gateway Bridge, Mosquitto, Redis y PostgreSQL.
- `tests/`: pruebas Python agrupadas por funciones, límites, pares y escenarios; las pruebas Java están bajo `dashboard-java/src/test/`.

## Documentos, según necesidad

- Ejecución local y comandos: `COMANDOS_INICIO.md`.
- Arquitectura y protocolo implementado: `docs/TECHNICAL_DOCUMENTATION.md` (copia también en `dashboard-java/docs/`).
- Requerimientos del dashboard: `DASHBOARD_SDN_REQUERIMIENTOS.md`.
- Firmware: `src/firmware/README_FIRMWARE.md`.
- Consultas Redis del pipeline Python: `CONSULTAS_REDIS.md`.
- `SDN_context.md` contiene antecedentes y diseño de la arquitectura inicial; puede estar desactualizado frente al dashboard Java y firmware Heltec actuales.

## Flujo embebido (resumen)

1. Cada Heltec V3 inicia RadioLib/LoRaWAN US915, configura credenciales OTAA y se une a ChirpStack; cada unidad necesita identidad y llaves únicas.
2. El firmware toma la medición, arma un payload binario little-endian y lo envía por LoRaWAN. ChirpStack entrega el evento por MQTT al backend Java, que decodifica según DevEUI/`fPort`, guarda y publica la actualización al dashboard.
3. En downlink, el backend empaqueta un encabezado SDN de 32 bits; el firmware lo interpreta después de `sendReceive` y puede cambiar intervalo/modo de payload, ajustar parámetros, dormir o reiniciar.
4. El nodo de huevos (`firmware_egg/.../main_egg.ino`) recibe conteos de MaixCAM por UART con sincronía y CRC, responde ACK y prepara telemetría. Su modo standalone simulado está habilitado en el código (`MOCK_STANDALONE_MODE`).
5. El nodo de peso (`user_firmware/main/main.ino` y `scale_driver.h`) promedia lecturas ADC, aplica filtro EMA, calcula peso/calibración/tara/capacidad de tolva y muestra estado en OLED. Revisa el firmware desplegado para confirmar sensor y valores reales.
6. La variante en `src/firmware/heltec_v3/` es otro sketch de prueba: usa un DevEUI de ejemplo y genera payload de 204 bytes; no asumir que corresponde al firmware final de ambiente, huevos o peso.

Para entender una falla embebida, sigue `setup()` → lectura/periférico → empaquetado → `sendReceive()` → manejo del downlink en el mismo sketch. Confirma qué firmware se flasheó antes de editar, porque hay copias y variantes en varias carpetas.

## Backend Java (alcance actual)

`dashboard-java/` es una aplicación Spring Boot 3 / Java 21 que incluye frontend web estático, API REST, cliente MQTT, decoder binario, codec SDN, WebSocket/STOMP y persistencia PostgreSQL. El servicio MQTT recibe `application/+/device/+/event/up`, clasifica/decodifica telemetría por DevEUI y `fPort`, mantiene estado e historial recientes, persiste registros y publica actualizaciones para la interfaz. También genera downlinks SDN y los publica en `application/{appId}/device/{devEui}/command/down`.

El backend contiene gestión de dispositivos, calibración de balanza, módulos de visión/escala/clima, autenticación/RBAC, API keys, rate limiting y auditoría. Las pruebas Java están en `dashboard-java/src/test/java/` y cubren protocolo, persistencia, seguridad e integración. La mera existencia de pruebas/reportes en `target/` no confirma el estado actual de ejecución; no se han ejecutado en esta revisión.

Puntos de entrada Java: `DashboardApplication.java`; MQTT y uplink/downlink: `service/MqttClientService.java`; protocolo: `protocol/TelemetryDecoder.java` y `protocol/SdnHeaderPacker.java`; API principal: `controller/DashboardRestController.java`; persistencia: `service/PersistenceService.java` y `entity/`/`repository/`.

## Reglas prácticas para trabajar

1. Identifica primero el componente afectado y sigue su flujo hasta sus interfaces (MQTT/API/DB/protocolo/firmware).
2. Para comportamiento actual, confía en el código y las pruebas del componente; usa requerimientos y notas como contexto, no como prueba de implementación.
3. El dashboard modela datos de ambiente, balanza y contador de huevos. `TelemetryDecoder` discrimina el contador de huevos por DevEUI/nombre o `fPort`; confirma las identidades reales de los tres nodos antes de depender de esa clasificación.
4. El encabezado SDN tiene 32 bits (4 bytes), pero verifica el codec compartido y sus pruebas antes de cambiar campos o endianness.
5. Hay archivos modificados y artefactos locales no versionados en el árbol de trabajo. Antes de editar, consulta `git status`; no sobrescribas cambios del usuario ni limpies artefactos sin instrucción.
6. Para reducir lectura, consulta símbolos puntuales con `rg` y abre solo los archivos relevantes; no leas ambos documentos técnicos completos salvo que la tarea abarque arquitectura.

## Inicio recomendado por tarea

1. `git status --short`.
2. Leer este contexto y ubicar el componente en el mapa.
3. Buscar el endpoint, servicio, función o protocolo implicado con `rg`.
4. Consultar la guía específica indicada arriba y pruebas cercanas si se necesitan.
