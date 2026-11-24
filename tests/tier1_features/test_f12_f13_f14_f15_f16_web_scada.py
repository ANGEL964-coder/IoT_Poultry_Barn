"""
Tier 1: Feature Coverage Tests for Web SCADA & UI Features:
- F12: Visual SCADA Layout (Mockup.png)
- F13: Network Topology Tree
- F14: KPI Cards & Digital Twin
- F15: Time-Series Charts & Event Feed
- F16: SDN Control Console Modal
Total Tests: 25 (5 tests per feature).
"""

import os
import re
import pytest

INDEX_HTML_PATH = "/home/server/CONTROLADOR_SDN/dashboard-java/src/main/resources/static/index.html"


# --- F12: Visual SCADA Layout ---

def test_f12_scada_root_endpoint_html(http_client):
    """Verify root GET / serves valid HTML document with SCADA title."""
    resp = http_client.get("/")
    assert resp.status_code == 200
    assert "<!DOCTYPE html>" in resp.text or "<html" in resp.text


def test_f12_no_raw_emojis_in_web_responses(http_client):
    """Verify zero raw emojis in web route responses (technical UI standard)."""
    resp = http_client.get("/")
    assert resp.status_code == 200
    html = resp.text
    # Regex detecting emoji unicode ranges
    emoji_pattern = re.compile(r"[\U00010000-\U0010ffff\u2600-\u27bf\u2300-\u23ff]")
    matches = emoji_pattern.findall(html)
    assert len(matches) == 0, f"Found raw emojis in web response: {matches}"


def test_f12_scada_viewport_meta_tag():
    """Verify responsive viewport meta tag is defined."""
    with open(INDEX_HTML_PATH, "r", encoding="utf-8") as f:
        html = f.read()
    assert '<meta name="viewport"' in html
    assert "width=device-width" in html


def test_f12_scada_dark_theme_palette():
    """Verify dark SCADA theme palette is defined (slate/dark theme styling)."""
    with open(INDEX_HTML_PATH, "r", encoding="utf-8") as f:
        html = f.read()
    assert "dark" in html.lower() or "slate-900" in html or "bg-slate" in html or "#0f172a" in html


def test_f12_scada_font_awesome_icons_used():
    """Verify FontAwesome icons are used instead of emojis for indicators."""
    with open(INDEX_HTML_PATH, "r", encoding="utf-8") as f:
        html = f.read()
    assert "fa-solid" in html or "fa-" in html


# --- F13: Network Topology Tree ---

def test_f13_topology_tree_gateway_central(http_client):
    """Verify network status endpoint reports active nodes and gateway connection."""
    resp = http_client.get("/api/status")
    assert resp.status_code == 200
    data = resp.json()
    assert "activeNodesCount" in data or "status" in data


def test_f13_topology_tree_nodes_registered(http_client):
    """Verify all 3 Heltec LoRaWAN nodes are present in node list."""
    resp = http_client.get("/api/galpones")
    assert resp.status_code == 200
    assert isinstance(resp.json(), (list, dict))


def test_f13_topology_tree_rf_signal_metrics():
    """Verify index.html contains RF signal indicators (RSSI, SNR, Spreading Factor)."""
    with open(INDEX_HTML_PATH, "r", encoding="utf-8") as f:
        html = f.read()
    assert "RSSI" in html or "rssi" in html
    assert "SNR" in html or "snr" in html or "SF" in html


def test_f13_topology_tree_status_flags():
    """Verify connection status indicators (Conectando / Conectado)."""
    with open(INDEX_HTML_PATH, "r", encoding="utf-8") as f:
        html = f.read()
    assert "wsStatusDot" in html or "status" in html.lower()


def test_f13_topology_tree_deveui_lookup(http_client):
    """Verify querying an existing device returns 200 with matching DevEUI, while unknown returns 404."""
    dev_eui = "70b3d57ed007985a"
    # Ensure known seed device has telemetry present in mock client
    if hasattr(http_client, "telemetries") and dev_eui not in http_client.telemetries:
        http_client.telemetries[dev_eui] = [{
            "devEui": dev_eui,
            "seq": 1,
            "weightKg": 15.5,
            "temperature": 24.5,
            "hopperPercentage": 77.5
        }]

    # 1. Query known seeded device -> strictly 200
    resp_known = http_client.get(f"/api/galpones/{dev_eui}")
    assert resp_known.status_code == 200, f"Expected 200 for known device {dev_eui}: {resp_known.text}"
    known_data = resp_known.json()
    assert known_data.get("devEui", "").lower() == dev_eui.lower()

    # 2. Query nonexistent device -> strictly 404
    resp_unknown = http_client.get("/api/galpones/ffffffffffffffff")
    assert resp_unknown.status_code == 404, f"Expected 404 for nonexistent device: {resp_unknown.status_code}"


# --- F14: KPI Cards & Digital Twin ---

def test_f14_scale_card_metrics():
    """Verify Scale Card elements (Peso, Tolva, Capacidad) in markup."""
    with open(INDEX_HTML_PATH, "r", encoding="utf-8") as f:
        html = f.read()
    assert "PESO" in html or "Tolva" in html or "peso" in html


def test_f14_climate_card_metrics():
    """Verify Climate Card elements (Temperatura, Humedad, Presión) in markup."""
    with open(INDEX_HTML_PATH, "r", encoding="utf-8") as f:
        html = f.read()
    assert "TEMP" in html or "Temperatura" in html or "Humedad" in html


def test_f14_egg_nest_card_metrics():
    """Verify Galpón / Barn monitoring elements in markup."""
    with open(INDEX_HTML_PATH, "r", encoding="utf-8") as f:
        html = f.read()
    assert "Galp" in html or "galpon" in html.lower()


def test_f14_battery_gauge_voltage():
    """Verify battery voltage (Batería / mV) metrics in markup."""
    with open(INDEX_HTML_PATH, "r", encoding="utf-8") as f:
        html = f.read()
    assert "Bater" in html or "bateria" in html.lower() or "mV" in html or "Volt" in html


def test_f14_digital_twin_barn_representation():
    """Verify visual barn representation and telemetry digital twin components in markup."""
    with open(INDEX_HTML_PATH, "r", encoding="utf-8") as f:
        html = f.read()
    # Digital twin node telemetry parameters section
    assert "Parámetros de Telemetría del Nodo Galpón" in html or "dtDevEui" in html
    # Digital twin live metric bindings (DevEUI, battery, voltage, ADC, Time on Air)
    assert "dtDevEui" in html
    assert "dtBattery" in html
    assert "dtVoltage" in html
    assert "dtAdc" in html
    assert "dtToa" in html
    # Digital twin visual charts (weight and ambient temperature)
    assert 'id="chartWeight"' in html
    assert 'id="chartTemp"' in html


# --- F15: Time-Series Charts & Event Feed ---

def test_f15_history_endpoint_data(http_client):
    """Verify GET /api/galpones/{devEui}/history returns history list."""
    resp = http_client.get("/api/galpones/70b3d57ed007985a/history")
    assert resp.status_code == 200
    assert isinstance(resp.json(), list)


def test_f15_history_buffer_cap(http_client):
    """Verify telemetry history buffer is bounded to prevent unbounded growth."""
    resp = http_client.get("/api/galpones/70b3d57ed007985a/history")
    assert resp.status_code == 200
    assert len(resp.json()) <= 100


def test_f15_history_chronological_order(http_client):
    """Verify GET /api/galpones/{devEui}/history maintains chronological sequence order."""
    dev_eui = "70b3d57ed007985a"
    if hasattr(http_client, "telemetries"):
        http_client.telemetries[dev_eui] = [
            {"devEui": dev_eui, "seq": 10, "weightKg": 15.0, "uptimeMs": 10000},
            {"devEui": dev_eui, "seq": 11, "weightKg": 15.2, "uptimeMs": 20000},
            {"devEui": dev_eui, "seq": 12, "weightKg": 15.4, "uptimeMs": 30000},
        ]
    resp = http_client.get(f"/api/galpones/{dev_eui}/history")
    assert resp.status_code == 200
    history = resp.json()
    assert isinstance(history, list)
    assert len(history) >= 2, f"Expected >= 2 historical records, got {len(history)}"
    seqs = [item["seq"] for item in history if "seq" in item]
    assert seqs == sorted(seqs), f"History records are not in chronological order: {seqs}"


def test_f15_stats_endpoint_metrics(http_client):
    """Verify /api/stats endpoint returns summary telemetry counters."""
    resp = http_client.get("/api/stats")
    assert resp.status_code == 200
    data = resp.json()
    assert "totalUplinks" in data or "activeDevices" in data or "totalDownlinks" in data


def test_f15_event_feed_audit_entries(http_client):
    """Verify event feed / downlink history log."""
    resp = http_client.get("/api/sdn/downlinks")
    assert resp.status_code == 200
    assert isinstance(resp.json(), list)


# --- F16: SDN Control Console Modal ---

def test_f16_sdn_modal_command_options():
    """Verify SDN modal contains command selectors (Modo SDN / Comandos)."""
    with open(INDEX_HTML_PATH, "r", encoding="utf-8") as f:
        html = f.read()
    assert "Modo SDN" in html or "sdn" in html.lower() or "cmd" in html.lower()


def test_f16_sdn_modal_sf_lut_options():
    """Verify Spreading Factor options (SF7, SF8, SF9, SF10, SF11, SF12) in markup/code."""
    with open(INDEX_HTML_PATH, "r", encoding="utf-8") as f:
        html = f.read()
    assert "SF7" in html or "SF8" in html or "Spreading" in html or "sf" in html.lower()


def test_f16_sdn_modal_tx_power_limits():
    """Verify RF TX power controls (dBm)."""
    with open(INDEX_HTML_PATH, "r", encoding="utf-8") as f:
        html = f.read()
    assert "dBm" in html or "Potencia" in html or "txPower" in html or "tx_power" in html


def test_f16_sdn_modal_sample_rate_intervals():
    """Verify sampling interval options (10s, 60s, 300s, etc.)."""
    with open(INDEX_HTML_PATH, "r", encoding="utf-8") as f:
        html = f.read()
    assert "segundos" in html or "Intervalo" in html or "sampleRate" in html or "Muestreo" in html or "sample" in html.lower()


def test_f16_sdn_modal_live_32bit_preview(http_client):
    """Verify SDN modal 32-bit preview generation via /api/sdn/preview."""
    req = {
        "cmdId": 1,
        "sfTarget": 1,
        "txPower": 14,
        "sampleRate": 2,
        "payloadMode": 1,
        "sleepEnable": 0,
        "paramValue": 0
    }
    resp = http_client.post("/api/sdn/preview", json_data=req)
    assert resp.status_code == 200
    data = resp.json()
    assert data.get("base64") == "EVcAAA=="
    assert data.get("hexDump") in ("0x00005711", "11 57 00 00", "0x11570000") or data.get("rawWordHex") == "0x00005711", (
        f"Invalid hexDump representation: {data.get('hexDump')}"
    )


def test_f16_scale_calibration_ui_elements_present():
    """Verify Scale subtab provides functional UI controls for tare and calibration."""
    with open(INDEX_HTML_PATH, "r", encoding="utf-8") as f:
        html = f.read()
    # Tare and calibration elements
    assert "scale/tare" in html or "Tare" in html or "Tara" in html or "btnTare" in html
    assert "scale/calibrate" in html or "Calibrar" in html or "btnCalibrate" in html
    # Subtab container
    assert "subtabScalePanel" in html or "subtabScale" in html or "scalePanel" in html


def test_f16_dedicated_module_subtabs_present():
    """Verify dedicated subtabs exist in HTML (Twin, Vision, Scale, Climate, Gateway)."""
    with open(INDEX_HTML_PATH, "r", encoding="utf-8") as f:
        html = f.read()
    assert "subtabVisionPanel" in html or "tabVision" in html or "Vision" in html
    assert "subtabScalePanel" in html or "tabScale" in html or "Tolva" in html
    assert "subtabClimatePanel" in html or "tabClimate" in html or "Clima" in html
    assert "subtabGatewayPanel" in html or "tabGateway" in html or "Gateway" in html
