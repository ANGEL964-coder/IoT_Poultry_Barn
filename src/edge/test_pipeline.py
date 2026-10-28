#!/usr/bin/env python3
"""
SDN-LoRa Edge Pipeline Validator (Phase 1)
------------------------------------------
Listens to ChirpStack v4 Uplink MQTT events (application/+/device/+/event/up),
extracts DevEUI, RSSI, SNR, and decoded payload bytes (e.g. 204 bytes telemetry),
and atomically updates the Redis Global State Table under `nodo:<DEV_EUI>`.
"""

import os
import sys
import json
import signal
import base64
import logging
import argparse
from datetime import datetime, timezone
from typing import Dict, Any, Optional

import redis
import paho.mqtt.client as mqtt

# Configure structured logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] [EdgePipeline] %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S"
)
logger = logging.getLogger("EdgePipeline")


class EdgePipelineValidator:
    def __init__(
        self,
        mqtt_host: str = "localhost",
        mqtt_port: int = 1883,
        redis_host: str = "localhost",
        redis_port: int = 6379,
        redis_db: int = 0,
        mqtt_topic: str = "application/+/device/+/event/up"
    ):
        self.mqtt_host = mqtt_host
        self.mqtt_port = mqtt_port
        self.mqtt_topic = mqtt_topic

        self.redis_client = redis.Redis(
            host=redis_host,
            port=redis_port,
            db=redis_db,
            decode_responses=True
        )

        # Set up MQTT client (supporting both paho-mqtt v1 and v2 callbacks)
        self.mqtt_client = mqtt.Client(
            callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
            client_id="sdn_edge_pipeline_validator"
        )
        self.mqtt_client.on_connect = self._on_connect
        self.mqtt_client.on_message = self._on_message
        self.mqtt_client.on_disconnect = self._on_disconnect

        self.running = False
        self.processed_counter = 0

    def test_redis_connection(self) -> bool:
        """Verifies active connectivity to the Redis database."""
        try:
            self.redis_client.ping()
            logger.info("Connected to Redis at %s:%d", self.redis_client.connection_pool.connection_args["host"],
                        self.redis_client.connection_pool.connection_args["port"])
            return True
        except Exception as e:
            logger.error("Failed to connect to Redis: %s", e)
            return False

    def _on_connect(self, client, userdata, flags, reason_code, properties=None):
        if reason_code == 0:
            logger.info("Successfully connected to MQTT Broker (%s:%d)", self.mqtt_host, self.mqtt_port)
            logger.info("Subscribing to Uplink topic: %s", self.mqtt_topic)
            client.subscribe(self.mqtt_topic, qos=1)
        else:
            logger.error("Failed to connect to MQTT broker, reason_code=%s", reason_code)

    def _on_disconnect(self, client, userdata, flags, reason_code, properties=None):
        if self.running:
            logger.warning("Disconnected from MQTT broker (reason: %s). Reconnecting...", reason_code)

    def _on_message(self, client, userdata, message):
        """Processes incoming LoRaWAN uplink events published by ChirpStack."""
        topic = message.topic
        payload_str = message.payload.decode("utf-8", errors="ignore")

        try:
            payload_json = json.loads(payload_str)
        except json.JSONDecodeError:
            logger.warning("Received invalid non-JSON payload on topic: %s", topic)
            return

        self._process_uplink(topic, payload_json)

    def _process_uplink(self, topic: str, data: Dict[str, Any]):
        """
        Parses ChirpStack v4 JSON uplink frame and commits state atomically to Redis.
        """
        # 1. DevEUI resolution: check deviceInfo or fallback to topic
        dev_eui = (
            data.get("deviceInfo", {}).get("devEui") or
            data.get("devEui") or
            self._extract_deveui_from_topic(topic)
        )
        if not dev_eui:
            logger.error("Could not determine DevEUI from topic '%s' or message payload", topic)
            return

        dev_eui = dev_eui.lower()

        # 2. Extract RF Metrics (RSSI & SNR)
        rx_info_list = data.get("rxInfo", [])
        rssi: Optional[float] = None
        snr: Optional[float] = None
        gateway_id: Optional[str] = None

        if rx_info_list and isinstance(rx_info_list, list):
            # Pick primary / highest RSSI gateway reception
            primary_rx = max(rx_info_list, key=lambda rx: rx.get("rssi", -999))
            rssi = float(primary_rx.get("rssi", 0.0))
            snr = float(primary_rx.get("snr", 0.0))
            gateway_id = primary_rx.get("gatewayId")

        # 3. Extract LoRa Frame info
        fcnt = data.get("fCnt", 0)
        fport = data.get("fPort", 0)
        data_b64 = data.get("data", "")

        # 4. Decode Application Payload
        payload_bytes = b""
        payload_len = 0
        payload_hex = ""
        if data_b64:
            try:
                payload_bytes = base64.b64decode(data_b64)
                payload_len = len(payload_bytes)
                payload_hex = payload_bytes.hex().upper()
            except Exception as e:
                logger.error("Error decoding base64 payload from node %s: %s", dev_eui, e)

        # 5. Build State Record
        now = datetime.now(timezone.utc)
        iso_timestamp = now.isoformat()
        epoch_timestamp = int(now.timestamp())

        redis_key = f"nodo:{dev_eui}"

        # 6. ATOMIC Redis HSET operation
        state_mapping = {
            "dev_eui": dev_eui,
            "rssi": str(rssi) if rssi is not None else "N/A",
            "snr": str(snr) if snr is not None else "N/A",
            "gateway_id": gateway_id or "N/A",
            "fcnt": str(fcnt),
            "fport": str(fport),
            "payload_len": str(payload_len),
            "payload_b64": data_b64,
            "payload_hex": payload_hex,
            "last_seen_iso": iso_timestamp,
            "last_seen_epoch": str(epoch_timestamp)
        }

        try:
            # Atomic transaction via Redis pipeline
            pipeline = self.redis_client.pipeline(transaction=True)
            pipeline.hset(redis_key, mapping=state_mapping)
            # Maintain a time-indexed log/history list (capped to last 50 entries)
            history_entry = json.dumps({
                "ts": epoch_timestamp,
                "fcnt": fcnt,
                "rssi": rssi,
                "snr": snr,
                "len": payload_len
            })
            pipeline.lpush(f"{redis_key}:history", history_entry)
            pipeline.ltrim(f"{redis_key}:history", 0, 49)
            pipeline.execute()

            self.processed_counter += 1

            # Detailed Visual Logging
            payload_badge = (
                f"\033[92m[204 Bytes MATCH]\033[0m" if payload_len == 204
                else f"\033[93m[{payload_len} Bytes]\033[0m"
            )

            print("\n" + "=" * 65)
            print(f"📡 \033[1;36m[UPLINK EVENT #{self.processed_counter}]\033[0m Node: \033[1;32m{dev_eui}\033[0m")
            print(f"   ├─ Timestamp:    {iso_timestamp}")
            print(f"   ├─ Frame Cnt:    fCnt={fcnt} (Port: {fport})")
            print(f"   ├─ RF Quality:   RSSI={rssi} dBm | SNR={snr} dB (Gateway: {gateway_id})")
            print(f"   ├─ Payload Size: {payload_badge}")
            if payload_len > 0:
                hex_preview = payload_hex[:32] + "..." if len(payload_hex) > 32 else payload_hex
                print(f"   ├─ Hex Dump:     {hex_preview}")
            print(f"   └─ \033[1;35mRedis Hash:\033[0m   HSET '{redis_key}' (Atomic Commit OK)")
            print("=" * 65)

        except Exception as e:
            logger.error("Failed to commit node %s state to Redis: %s", dev_eui, e)

    @staticmethod
    def _extract_deveui_from_topic(topic: str) -> Optional[str]:
        # Topic pattern: application/<app_id>/device/<dev_eui>/event/up
        parts = topic.split("/")
        if len(parts) >= 5 and parts[0] == "application" and parts[2] == "device":
            return parts[3]
        return None

    def start(self):
        """Starts the pipeline event loop."""
        self.running = True

        logger.info("Starting SDN Edge Pipeline Validator...")
        self.test_redis_connection()

        try:
            self.mqtt_client.connect(self.mqtt_host, self.mqtt_port, keepalive=60)
            self.mqtt_client.loop_start()

            logger.info("Pipeline listening on '%s'. Press Ctrl+C to stop.", self.mqtt_topic)
            while self.running:
                import time
                time.sleep(0.5)

        except KeyboardInterrupt:
            logger.info("Keyboard interrupt received.")
        finally:
            self.stop()

    def stop(self):
        """Gracefully shuts down MQTT client and resources."""
        if not self.running:
            return
        self.running = False
        logger.info("Stopping MQTT client loop...")
        self.mqtt_client.loop_stop()
        self.mqtt_client.disconnect()
        logger.info("Pipeline stopped cleanly. Total uplinks processed: %d", self.processed_counter)


def main():
    parser = argparse.ArgumentParser(
        description="SDN-LoRa Edge Pipeline: ChirpStack MQTT -> Redis Global State"
    )
    parser.add_argument("--mqtt-host", default=os.getenv("MQTT_HOST", "localhost"), help="Mosquitto broker IP/hostname")
    parser.add_argument("--mqtt-port", type=int, default=int(os.getenv("MQTT_PORT", 1883)), help="Mosquitto broker port")
    parser.add_argument("--redis-host", default=os.getenv("REDIS_HOST", "localhost"), help="Redis server IP/hostname")
    parser.add_argument("--redis-port", type=int, default=int(os.getenv("REDIS_PORT", 6379)), help="Redis server port")
    parser.add_argument("--topic", default="application/+/device/+/event/up", help="MQTT uplink topic wildcard")

    args = parser.parse_args()

    validator = EdgePipelineValidator(
        mqtt_host=args.mqtt_host,
        mqtt_port=args.mqtt_port,
        redis_host=args.redis_host,
        redis_port=args.redis_port,
        mqtt_topic=args.topic
    )

    def signal_handler(sig, frame):
        validator.stop()
        sys.exit(0)

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    validator.start()


if __name__ == "__main__":
    main()
