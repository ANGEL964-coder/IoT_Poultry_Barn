"""
Tier 3: Pairwise Combinatorial Integration Tests.
Pairwise interaction: Rapid Telemetry Bursts and Database Transactional Consistency.
Verifies atomic commit/rollback, sequence preservation, and concurrent DB query consistency.
Total Tests: 4 (Requirement: >= 4).
"""

import concurrent.futures
import time
import pytest


def test_tier3_rapid_telemetry_burst_transactions(db_conn):
    """Verify rapid consecutive insertions of telemetry frames within a transaction."""
    cur = db_conn.cursor()
    cur.execute(
        "CREATE TABLE IF NOT EXISTS telemetry_records ("
        "id SERIAL PRIMARY KEY, dev_eui VARCHAR(32) NOT NULL, seq BIGINT, "
        "battery_mv REAL, temperature REAL, humidity REAL, pressure REAL, weight_kg REAL);"
    )

    dev_eui = "70b3d57ed007985a"
    batch_size = 20

    for i in range(batch_size):
        cur.execute(
            "INSERT INTO telemetry_records (dev_eui, seq, battery_mv, temperature, humidity, pressure, weight_kg) "
            "VALUES (%s, %s, %s, %s, %s, %s, %s);",
            (dev_eui, 500 + i, 3300.0, 24.0 + (i * 0.1), 55.0, 1013.0, 15.0 - (i * 0.05))
        )

    db_conn.commit()

    # Query count
    cur.execute("SELECT COUNT(*) FROM telemetry_records WHERE dev_eui = %s AND seq >= 500;", (dev_eui,))
    count = cur.fetchone()[0]
    cur.close()
    assert count >= batch_size


def test_tier3_transactional_rollback_on_error(db_conn):
    """Verify that an aborted transaction rolls back uncommitted records cleanly."""
    cur = db_conn.cursor()
    cur.execute(
        "CREATE TABLE IF NOT EXISTS telemetry_records ("
        "id SERIAL PRIMARY KEY, dev_eui VARCHAR(32) NOT NULL, seq BIGINT, "
        "battery_mv REAL, temperature REAL, humidity REAL, pressure REAL, weight_kg REAL);"
    )

    dev_eui = "70b3d57ed007985a"
    # Insert a record without commit
    cur.execute(
        "INSERT INTO telemetry_records (dev_eui, seq, battery_mv, temperature, humidity, pressure, weight_kg) "
        "VALUES (%s, %s, %s, %s, %s, %s, %s);",
        (dev_eui, 999999, 3300.0, 24.0, 55.0, 1013.0, 10.0)
    )

    # Rollback
    db_conn.rollback()

    # Verify record was not committed
    cur.execute("SELECT COUNT(*) FROM telemetry_records WHERE seq = 999999;")
    count = cur.fetchone()[0]
    cur.close()
    assert count == 0


def test_tier3_concurrent_db_readers_and_writers(db_conn):
    """Verify concurrent reads and writes do not deadlock or raise serialization failure."""
    dev_eui = "766979864b726282"

    def write_task(seq_id: int):
        c = db_conn.cursor()
        c.execute(
            "INSERT INTO telemetry_records (dev_eui, seq, battery_mv, temperature, humidity, pressure, weight_kg) "
            "VALUES (%s, %s, %s, %s, %s, %s, %s);",
            (dev_eui, 2000 + seq_id, 3200.0, 25.0, 50.0, 1010.0, 12.0)
        )
        db_conn.commit()
        c.close()
        return True

    def read_task():
        c = db_conn.cursor()
        c.execute("SELECT COUNT(*) FROM telemetry_records WHERE dev_eui = %s;", (dev_eui,))
        res = c.fetchone()[0]
        c.close()
        return res >= 0

    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as executor:
        write_futures = [executor.submit(write_task, i) for i in range(10)]
        read_futures = [executor.submit(read_task) for _ in range(10)]

        for f in concurrent.futures.as_completed(write_futures + read_futures):
            assert f.result() is True


def test_tier3_monotonic_sequence_order_preserved(db_conn):
    """Verify sequence numbers in persisted telemetry frames preserve monotonic ordering."""
    cur = db_conn.cursor()
    dev_eui = "a84041000181cafe"
    # Clean previous run state for isolation
    cur.execute("DELETE FROM telemetry_records WHERE dev_eui = %s AND seq BETWEEN 10 AND 14;", (dev_eui,))
    db_conn.commit()

    for s in (10, 11, 12, 13, 14):
        cur.execute(
            "INSERT INTO telemetry_records (dev_eui, seq, battery_mv, temperature, humidity, pressure, weight_kg) "
            "VALUES (%s, %s, %s, %s, %s, %s, %s);",
            (dev_eui, s, 3300.0, 24.0, 50.0, 1013.0, 10.0)
        )
    db_conn.commit()

    cur.execute("SELECT seq FROM telemetry_records WHERE dev_eui = %s AND seq BETWEEN 10 AND 14 ORDER BY seq ASC;", (dev_eui,))
    rows = cur.fetchall()
    seqs = [r[0] for r in rows]
    cur.close()
    assert seqs == [10, 11, 12, 13, 14]


def test_tier3_delete_rollback_and_commit_transaction_semantics(db_conn):
    """Verify that uncommitted DELETE statements restore committed rows upon rollback and persist upon commit."""
    cur = db_conn.cursor()
    cur.execute("CREATE TABLE IF NOT EXISTS tx_del_test (id INT, label VARCHAR(32));")
    cur.execute("INSERT INTO tx_del_test (id, label) VALUES (%s, %s);", (1, "alpha"))
    cur.execute("INSERT INTO tx_del_test (id, label) VALUES (%s, %s);", (2, "beta"))
    db_conn.commit()

    # Verify both exist
    cur.execute("SELECT COUNT(*) FROM tx_del_test;")
    assert cur.fetchone() == (2,)

    # Uncommitted delete of id=1
    cur.execute("DELETE FROM tx_del_test WHERE id = 1;")
    assert cur.rowcount == 1

    # In transaction, id=1 should be invisible
    cur.execute("SELECT COUNT(*) FROM tx_del_test WHERE id = 1;")
    assert cur.fetchone() == (0,)

    # Rollback transaction
    db_conn.rollback()

    # Row id=1 must be restored
    cur.execute("SELECT COUNT(*) FROM tx_del_test WHERE id = 1;")
    assert cur.fetchone() == (1,)
    cur.execute("SELECT COUNT(*) FROM tx_del_test;")
    assert cur.fetchone() == (2,)

    # Uncommitted delete of id=2 followed by commit
    cur.execute("DELETE FROM tx_del_test WHERE id = 2;")
    assert cur.rowcount == 1
    db_conn.commit()

    # Row id=2 permanently removed
    cur.execute("SELECT COUNT(*) FROM tx_del_test WHERE id = 2;")
    assert cur.fetchone() == (0,)
    cur.execute("SELECT COUNT(*) FROM tx_del_test;")
    assert cur.fetchone() == (1,)
    cur.close()


def test_tier3_delete_semicolon_and_predicates_robustness(db_conn):
    """Verify DELETE queries with trailing semicolons, whitespace, and universal WHERE predicates."""
    cur = db_conn.cursor()
    cur.execute("CREATE TABLE IF NOT EXISTS pred_test (id INT, score REAL);")
    cur.execute("INSERT INTO pred_test (id, score) VALUES (%s, %s);", (100, 95.5))
    cur.execute("INSERT INTO pred_test (id, score) VALUES (%s, %s);", (101, 88.0))
    db_conn.commit()

    # Universal predicates in SELECT
    cur.execute("SELECT COUNT(*) FROM pred_test WHERE 1=1;")
    assert cur.fetchone() == (2,)
    cur.execute("SELECT COUNT(*) FROM pred_test WHERE TRUE;")
    assert cur.fetchone() == (2,)
    cur.execute("SELECT COUNT(*) FROM pred_test WHERE 1 = 1 AND score > 90;")
    assert cur.fetchone() == (1,)
    cur.execute("SELECT COUNT(*) FROM pred_test WHERE 1=0;")
    assert cur.fetchone() == (0,)

    # DELETE with trailing semicolon and whitespace
    cur.execute("DELETE FROM pred_test WHERE id = 100;   \n")
    assert cur.rowcount == 1
    db_conn.commit()

    cur.execute("SELECT COUNT(*) FROM pred_test WHERE id = 100;")
    assert cur.fetchone() == (0,)

    # Clean remaining
    cur.execute("DELETE FROM pred_test WHERE 1=1;")
    assert cur.rowcount == 1
    db_conn.commit()
    cur.execute("SELECT COUNT(*) FROM pred_test;")
    assert cur.fetchone() == (0,)
    cur.close()


def test_tier3_aggregate_functions_whitespace_tolerance(db_conn):
    """Verify aggregate function parsing tolerance for arbitrary whitespace formatting."""
    cur = db_conn.cursor()
    cur.execute("CREATE TABLE IF NOT EXISTS agg_ws_test (id INT, val REAL);")
    cur.execute("INSERT INTO agg_ws_test (id, val) VALUES (%s, %s);", (1, 10.0))
    cur.execute("INSERT INTO agg_ws_test (id, val) VALUES (%s, %s);", (2, 20.0))
    cur.execute("INSERT INTO agg_ws_test (id, val) VALUES (%s, %s);", (3, 30.0))
    db_conn.commit()

    cur.execute("SELECT COUNT (*) FROM agg_ws_test;")
    assert cur.fetchone() == (3,)

    cur.execute("SELECT MIN (val), MAX (val), AVG (val), SUM (val) FROM agg_ws_test;")
    row = cur.fetchone()
    assert row == (10.0, 30.0, 20.0, 60.0)

    # Empty table aggregate behavior
    cur.execute("DELETE FROM agg_ws_test WHERE 1=1;")
    db_conn.commit()
    cur.execute("SELECT COUNT (*) FROM agg_ws_test;")
    assert cur.fetchone() == (0,)
    cur.execute("SELECT MIN (val), MAX (val), AVG (val), SUM (val) FROM agg_ws_test;")
    assert cur.fetchone() == (None, None, None, None)
    cur.close()
