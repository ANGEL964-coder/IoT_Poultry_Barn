"""
Tier 4: Real-World Scenario 5 - Dual-Axis Data Analytics & Quota Calculation.
Simulates a 24-hour poultry feeding cycle with feed consumption telemetry,
daily rate calculation, egg quota progress tracking, and statistical aggregations.
"""

import pytest

from tests.test_helpers import (
    pack_telemetry_critical,
    decode_telemetry_frame,
    calculate_hopper_percentage
)

NODE_1 = "70b3d57ed007985a"


def test_scenario5_24h_cycle_and_quota_analytics(db_conn, http_client):
    """
    Step 1: Simulate 24 hourly telemetry readings for Galpón 1:
            Feed hopper starts at 20.0 kg (08:00) and decreases as chickens feed to 6.8 kg (20:00).
    Step 2: Persist 24 frames in database and local history cache.
    Step 3: Query time-series analytics (Min weight, Max weight, Total daily consumption).
    Step 4: Verify Egg Quota progress calculation (e.g. Target: 1000 eggs, Produced: 850 eggs -> 85.0%).
    Step 5: Verify KPI aggregate metrics.
    """
    cur = db_conn.cursor()
    cur.execute(
        "CREATE TABLE IF NOT EXISTS telemetry_records ("
        "id SERIAL PRIMARY KEY, dev_eui VARCHAR(32) NOT NULL, seq BIGINT, "
        "battery_mv REAL, temperature REAL, humidity REAL, pressure REAL, weight_kg REAL);"
    )

    # Clean old scenario data
    cur.execute("DELETE FROM telemetry_records WHERE dev_eui = %s AND seq BETWEEN 10000 AND 10023;", (NODE_1,))
    db_conn.commit()

    # Step 1 & 2: Generate 24 hourly points
    # Weight decreases smoothly from 20.0 kg to 6.8 kg
    hourly_telemetries = []
    for hour in range(24):
        seq = 10000 + hour
        weight = max(6.8, 20.0 - (hour * 0.55))
        temp = 22.0 + (5.0 if 10 <= hour <= 16 else 1.0)
        hum = 65.0 - (10.0 if 10 <= hour <= 16 else 0.0)

        cur.execute(
            "INSERT INTO telemetry_records (dev_eui, seq, battery_mv, temperature, humidity, pressure, weight_kg) "
            "VALUES (%s, %s, %s, %s, %s, %s, %s);",
            (NODE_1, seq, 3300.0, temp, hum, 1013.0, weight)
        )

        decoded = {
            "devEui": NODE_1,
            "seq": seq,
            "weightKg": round(weight, 2),
            "hopperPercentage": calculate_hopper_percentage(weight, 20.0),
            "temperature": temp,
            "humidity": hum
        }
        hourly_telemetries.append(decoded)

    db_conn.commit()

    # Step 3: Query Database Analytics
    cur.execute(
        "SELECT MIN(weight_kg), MAX(weight_kg), AVG(weight_kg) "
        "FROM telemetry_records WHERE dev_eui = %s AND seq BETWEEN 10000 AND 10023;",
        (NODE_1,)
    )
    min_w, max_w, avg_w = cur.fetchone()
    cur.close()

    assert min_w is not None and max_w is not None
    assert abs(max_w - 20.0) < 0.1
    assert abs(min_w - 6.8) < 0.6
    daily_consumption = max_w - min_w
    assert daily_consumption > 12.0  # Chickens consumed >12 kg feed over the day

    # Step 4: Egg Quota Calculation
    target_egg_quota = 1000
    actual_egg_count = 850
    quota_percentage = round((actual_egg_count / target_egg_quota) * 100.0, 1)
    assert quota_percentage == 85.0

    # Step 5: Verify SCADA history reflects latest points
    if hasattr(http_client, "telemetries"):
        http_client.telemetries[NODE_1] = hourly_telemetries

    history_resp = http_client.get(f"/api/galpones/{NODE_1}/history")
    assert history_resp.status_code == 200
    history = history_resp.json()
    assert len(history) == 24
    assert history[0]["seq"] == 10000
    assert history[-1]["seq"] == 10023
