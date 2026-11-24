"""
Shared Pytest Configuration and Dual-Mode Fixtures for E2E Testing Track.
Supports:
1. Live network mode (when Mosquitto 1883, PostgreSQL 5432, Spring Boot 8085, or Claude socket exist).
2. Isolated high-fidelity mock fixtures for deterministic test runs.
"""

import base64
import json
import os
import queue
import re
import socket
import struct
import tempfile
import threading
import time
from typing import Any, Callable, Dict, List, Optional, Set, Tuple, Union

import psycopg2
import pytest
import requests

from tests.test_helpers import (
    pack_sdn_command, unpack_sdn_command, to_base64, from_base64,
    decode_telemetry_frame, make_chirpstack_downlink, discover_claude_token
)

# Network Port Probing Helpers

def is_port_open(host: str, port: int, timeout: float = 0.5) -> bool:
    """Check if a TCP port is open and reachable."""
    try:
        with socket.create_connection((host, port), timeout=timeout):
            return True
    except (socket.timeout, ConnectionRefusedError, OSError):
        return False


def is_unix_socket_open(path: str, timeout: float = 0.5) -> bool:
    """Check if an AF_UNIX socket is active and accepts connections."""
    if not os.path.exists(path):
        return False
    try:
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.settimeout(timeout)
        s.connect(path)
        s.close()
        return True
    except (socket.timeout, ConnectionRefusedError, OSError):
        return False


# --- High-Fidelity Mock Fixtures ---

class MockMqttMessage:
    def __init__(self, topic: str, payload: Union[bytes, str], qos: int = 0, retain: bool = False):
        self.topic = topic
        if isinstance(payload, str):
            self.payload = payload.encode("utf-8")
        else:
            self.payload = bytes(payload)
        self.qos = qos
        self.retain = retain


class MockMqttClient:
    """High-fidelity in-memory MQTT client with wildcard topic matching."""
    def __init__(self, client_id: str = "mock-client"):
        self.client_id = client_id
        self.connected = False
        self.subscriptions: Dict[str, int] = {}
        self.published_messages: List[MockMqttMessage] = []
        self.on_connect: Optional[Callable] = None
        self.on_disconnect: Optional[Callable] = None
        self.on_message: Optional[Callable] = None
        self._lock = threading.Lock()
        self.message_queue: "queue.Queue[MockMqttMessage]" = queue.Queue()

    def connect(self, host: str = "127.0.0.1", port: int = 1883, keepalive: int = 60) -> int:
        self.connected = True
        if self.on_connect:
            self.on_connect(self, None, {}, 0)
        return 0

    def disconnect(self) -> int:
        self.connected = False
        if self.on_disconnect:
            self.on_disconnect(self, None, 0)
        return 0

    def is_connected(self) -> bool:
        return self.connected

    def subscribe(self, topic: str, qos: int = 0) -> Tuple[int, int]:
        with self._lock:
            self.subscriptions[topic] = qos
        return (0, 1)

    def unsubscribe(self, topic: str) -> Tuple[int, int]:
        with self._lock:
            self.subscriptions.pop(topic, None)
        return (0, 1)

    def publish(self, topic: str, payload: Union[bytes, str], qos: int = 1, retain: bool = False):
        msg = MockMqttMessage(topic, payload, qos, retain)
        with self._lock:
            self.published_messages.append(msg)
            self.message_queue.put(msg)

        # Dispatch to subscribers matching wildcard
        if self.on_message:
            for sub in list(self.subscriptions.keys()):
                if self._match_topic(sub, topic):
                    self.on_message(self, None, msg)
                    break

        class Result:
            rc = 0
            def wait_for_publish(self, timeout=None):
                return True
        return Result()

    def loop_start(self):
        pass

    def loop_stop(self):
        pass

    @staticmethod
    def _match_topic(sub: str, topic: str) -> bool:
        """MQTT standard wildcard matching with '+' and '#'."""
        if sub == topic or sub == "#":
            return True
        sub_parts = sub.split("/")
        topic_parts = topic.split("/")
        for i, part in enumerate(sub_parts):
            if part == "#":
                return True
            if i >= len(topic_parts):
                return False
            if part != "+" and part != topic_parts[i]:
                return False
        return len(sub_parts) == len(topic_parts)


class MockDbCursor:
    def __init__(self, db: "MockDbConnection"):
        self.db = db
        self.last_query = ""
        self.last_params = None
        self._results: List[Any] = []
        self._idx = 0
        self.rowcount = 0

    def execute(self, query: str, params: Any = None):
        self.last_query = query
        self.last_params = params
        q = query.strip()
        q_upper = q.upper()

        with self.db.lock:
            if "SELECT 1" in q_upper or "SELECT VERSION()" in q_upper:
                self._results = [("PostgreSQL 14.24 mock",)]
                self.rowcount = 1
                self._idx = 0
                return

            if "INFORMATION_SCHEMA.TABLES" in q_upper:
                self._results = [(t,) for t in self.db.get_all_table_names()]
                self.rowcount = len(self._results)
                self._idx = 0
                return

            if q_upper.startswith("CREATE TABLE"):
                m = re.search(r'CREATE\s+TABLE\s+(?:IF\s+NOT\s+EXISTS\s+)?(\w+)', q, re.IGNORECASE)
                if m:
                    tbl = m.group(1).lower()
                    self.db.ensure_table(tbl)
                self._results = []
                self.rowcount = 0
                self._idx = 0
                return

            if q_upper.startswith("INSERT INTO"):
                m = re.search(r'INSERT\s+INTO\s+(\w+)\s*\(([^)]+)\)\s*VALUES\s*(?:\(([^)]+)\))?', q, re.IGNORECASE)
                if m:
                    tbl = m.group(1).lower()
                    cols = [c.strip().lower() for c in m.group(2).split(",")]
                    if params:
                        val_params = list(params)
                    elif m.group(3):
                        raw_vals = [v.strip() for v in m.group(3).split(",")]
                        val_params = []
                        for raw in raw_vals:
                            if raw.upper() == "NULL":
                                val_params.append(None)
                            elif raw.upper() == "TRUE":
                                val_params.append(True)
                            elif raw.upper() == "FALSE":
                                val_params.append(False)
                            elif (raw.startswith("'") and raw.endswith("'")) or (raw.startswith('"') and raw.endswith('"')):
                                val_params.append(raw[1:-1])
                            else:
                                try:
                                    if "." in raw:
                                        val_params.append(float(raw))
                                    else:
                                        val_params.append(int(raw))
                                except ValueError:
                                    val_params.append(raw)
                    else:
                        val_params = []
                    row_dict = dict(zip(cols, val_params))
                    self.db.insert_row(tbl, row_dict)
                    self.rowcount = 1
                else:
                    self.rowcount = 1
                self._results = []
                self._idx = 0
                return

            if q_upper.startswith("DELETE FROM"):
                m = re.search(r'DELETE\s+FROM\s+(\w+)(?:\s+WHERE\s+(.*?))?(?:;|\s*$)', q, re.IGNORECASE | re.DOTALL)
                if m:
                    tbl = m.group(1).lower()
                    where_clause = m.group(2)
                    if where_clause:
                        where_clause = where_clause.rstrip('; \t\n').strip()
                    formatted_where = self.db.format_query(where_clause, params) if where_clause else ""
                    deleted_count = self.db.delete_rows(tbl, formatted_where)
                    self.rowcount = deleted_count
                else:
                    self.rowcount = 0
                self._results = []
                self._idx = 0
                return

            if q_upper.startswith("UPDATE"):
                self.rowcount = 1
                self._results = []
                self._idx = 0
                return

            if q_upper.startswith("SELECT"):
                m_from = re.search(r'FROM\s+(\w+)(?:\s+WHERE\s+(.*?))?(?:\s+ORDER\s+BY\s+(.*?))?(?:;|\s*$)', q, re.IGNORECASE | re.DOTALL)
                if m_from:
                    tbl = m_from.group(1).lower()
                    where_clause = m_from.group(2)
                    order_clause = m_from.group(3)

                    rows = self.db.get_rows(tbl)

                    if where_clause:
                        formatted_where = self.db.format_query(where_clause.strip(), params)
                        rows = [r for r in rows if self.db.row_matches(r, formatted_where)]

                    if order_clause:
                        order_parts = order_clause.strip().split()
                        order_col = order_parts[0].lower()
                        descending = len(order_parts) > 1 and order_parts[1].upper() == "DESC"
                        rows.sort(key=lambda r: r.get(order_col, 0), reverse=descending)

                    m_select = re.search(r'SELECT\s+(.+?)\s+FROM', q, re.IGNORECASE | re.DOTALL)
                    if m_select:
                        select_expr = m_select.group(1).strip()
                        self._results = self.db.project_columns(rows, select_expr)
                    else:
                        self._results = []

                    self.rowcount = len(self._results)
                    self._idx = 0
                    return

            self._results = []
            self.rowcount = 0
            self._idx = 0

    def fetchone(self):
        if self._idx < len(self._results):
            row = self._results[self._idx]
            self._idx += 1
            return row
        return None

    def fetchall(self):
        res = self._results[self._idx:]
        self._idx = len(self._results)
        return res

    def close(self):
        pass


class MockDbConnection:
    """In-memory mock database connection for offline testing."""
    def __init__(self):
        self.lock = threading.RLock()
        self.committed_tables: Dict[str, List[Dict[str, Any]]] = {
            "devices": [
                {"deveui": "70b3d57ed007985a", "dev_eui": "70b3d57ed007985a", "name": "heltec-v3-poultry-01", "description": "Galpón 1 - Tolva y Clima", "active": True},
                {"deveui": "766979864b726282", "dev_eui": "766979864b726282", "name": "heltec-v3-poultry-02", "description": "Galpón 2 - Tolva y Clima", "active": True},
                {"deveui": "a84041000181cafe", "dev_eui": "a84041000181cafe", "name": "heltec-v3-poultry-03", "description": "Galpón 3 - Diagnóstico", "active": True},
            ],
            "api_keys": [
                {"id": 1, "key": "master-test-key-2026", "name": "Operator Admin", "active": True, "created_at": "2026-11-17T18:00:00Z"},
                {"id": 2, "key": "node-test-key-01", "name": "Galpon 1 Sensor", "active": True, "created_at": "2026-11-17T18:00:00Z"},
            ],
            "telemetry_records": [],
            "telemetry_history": [],
            "sdn_rule_events": [],
            "audit_logs": [],
        }
        self.uncommitted_tables: Dict[str, List[Dict[str, Any]]] = {k: [] for k in self.committed_tables}
        self.uncommitted_deletions: Dict[str, Set[int]] = {k: set() for k in self.committed_tables}
        self.autocommit = False

    @property
    def tables(self) -> Dict[str, List[Any]]:
        with self.lock:
            result = {}
            for tbl, rows in self.committed_tables.items():
                del_set = self.uncommitted_deletions.get(tbl, set())
                result[tbl] = [tuple(r.values()) for r in rows if id(r) not in del_set]
            return result

    def ensure_table(self, table: str):
        with self.lock:
            if table not in self.committed_tables:
                self.committed_tables[table] = []
                self.uncommitted_tables[table] = []
                self.uncommitted_deletions[table] = set()

    def get_all_table_names(self) -> List[str]:
        with self.lock:
            return list(self.committed_tables.keys())

    def insert_row(self, table: str, row: Dict[str, Any]):
        with self.lock:
            self.ensure_table(table)
            if self.autocommit:
                self.committed_tables[table].append(row)
            else:
                self.uncommitted_tables[table].append(row)

    def delete_rows(self, table: str, condition_str: str) -> int:
        with self.lock:
            self.ensure_table(table)
            count = 0
            if self.autocommit:
                if condition_str:
                    new_committed = []
                    for r in self.committed_tables[table]:
                        if self.row_matches(r, condition_str):
                            count += 1
                        else:
                            new_committed.append(r)
                    self.committed_tables[table] = new_committed
                else:
                    count = len(self.committed_tables[table])
                    self.committed_tables[table].clear()
                return count

            # Transactional (uncommitted) mode
            if condition_str:
                new_uncommitted = []
                for r in self.uncommitted_tables[table]:
                    if self.row_matches(r, condition_str):
                        count += 1
                    else:
                        new_uncommitted.append(r)
                self.uncommitted_tables[table] = new_uncommitted

                del_set = self.uncommitted_deletions[table]
                for r in self.committed_tables[table]:
                    if id(r) not in del_set and self.row_matches(r, condition_str):
                        del_set.add(id(r))
                        count += 1
            else:
                count += len(self.uncommitted_tables[table])
                self.uncommitted_tables[table].clear()

                del_set = self.uncommitted_deletions[table]
                for r in self.committed_tables[table]:
                    if id(r) not in del_set:
                        del_set.add(id(r))
                        count += 1
            return count

    def get_rows(self, table: str) -> List[Dict[str, Any]]:
        with self.lock:
            self.ensure_table(table)
            del_set = self.uncommitted_deletions.get(table, set())
            rows = [dict(r) for r in self.committed_tables[table] if id(r) not in del_set]
            rows.extend([dict(r) for r in self.uncommitted_tables[table]])
            return rows

    def format_query(self, query: str, params: Any) -> str:
        if not params:
            return query
        if isinstance(params, (list, tuple)):
            q = query
            for p in params:
                formatted = f"'{p}'" if isinstance(p, str) else ("NULL" if p is None else str(p))
                q = q.replace("%s", formatted, 1)
            return q
        return query

    def row_matches(self, row: Dict[str, Any], condition_str: str) -> bool:
        if not condition_str:
            return True
        between_pattern = re.compile(r'(\w+)\s+BETWEEN\s+(\S+)\s+AND\s+(\S+)', re.IGNORECASE)
        norm_condition = between_pattern.sub(r'\1 BETWEEN_VAL \2 TO \3', condition_str)
        clauses = re.split(r'\s+AND\s+', norm_condition, flags=re.IGNORECASE)
        for clause in clauses:
            clause_clean = clause.strip().rstrip(';').strip()
            if not clause_clean:
                continue
            clause_norm = re.sub(r'\s+', '', clause_clean).upper()
            if clause_norm in ("1=1", "TRUE") or re.match(r'^(\d+)=(\1)$', clause_norm):
                continue
            if clause_norm in ("1=0", "FALSE") or re.match(r'^(\d+)!=(\1)$', clause_norm):
                return False

            m_between = re.search(r'(\w+)\s+BETWEEN_VAL\s+(\S+)\s+TO\s+(\S+)', clause_clean, re.IGNORECASE)
            if m_between:
                col, low_str, high_str = m_between.groups()
                col = col.lower()
                val = row.get(col)
                if val is None:
                    return False
                try:
                    v = float(val)
                    low = float(low_str.strip().rstrip(';').strip("'\""))
                    high = float(high_str.strip().rstrip(';').strip("'\""))
                    if not (low <= v <= high):
                        return False
                except ValueError:
                    return False
                continue

            m_op = re.search(r'(\w+)\s*(>=|<=|!=|=|>|<)\s*(.+)', clause_clean)
            if m_op:
                col, op, target_str = m_op.groups()
                col = col.lower()
                val = row.get(col)
                target = target_str.strip().rstrip(';').strip("'\"")
                if val is None:
                    return False
                try:
                    v_num = float(val)
                    t_num = float(target)
                    if op == '=' and v_num != t_num:
                        return False
                    elif op == '!=' and v_num == t_num:
                        return False
                    elif op == '>=' and not (v_num >= t_num):
                        return False
                    elif op == '<=' and not (v_num <= t_num):
                        return False
                    elif op == '>' and not (v_num > t_num):
                        return False
                    elif op == '<' and not (v_num < t_num):
                        return False
                except ValueError:
                    v_str = str(val).strip().rstrip(';').strip("'\"")
                    if op == '=' and v_str != target:
                        return False
                    elif op == '!=' and v_str == target:
                        return False
                continue
        return True

    def project_columns(self, rows: List[Dict[str, Any]], expr: str) -> List[Tuple[Any, ...]]:
        is_aggregate = bool(re.search(r'\b(COUNT|MIN|MAX|AVG|SUM)\s*\(', expr, re.IGNORECASE))
        if is_aggregate:
            items = [i.strip() for i in expr.split(",")]
            agg_vals = []
            for item in items:
                if re.search(r'COUNT\s*\(', item, re.IGNORECASE):
                    agg_vals.append(len(rows))
                elif re.search(r'MIN\s*\(', item, re.IGNORECASE):
                    m = re.search(r'MIN\s*\(\s*(\w+)\s*\)', item, re.IGNORECASE)
                    col = m.group(1).lower() if m else ""
                    vals = [float(r[col]) for r in rows if r.get(col) is not None]
                    agg_vals.append(min(vals) if vals else None)
                elif re.search(r'MAX\s*\(', item, re.IGNORECASE):
                    m = re.search(r'MAX\s*\(\s*(\w+)\s*\)', item, re.IGNORECASE)
                    col = m.group(1).lower() if m else ""
                    vals = [float(r[col]) for r in rows if r.get(col) is not None]
                    agg_vals.append(max(vals) if vals else None)
                elif re.search(r'AVG\s*\(', item, re.IGNORECASE):
                    m = re.search(r'AVG\s*\(\s*(\w+)\s*\)', item, re.IGNORECASE)
                    col = m.group(1).lower() if m else ""
                    vals = [float(r[col]) for r in rows if r.get(col) is not None]
                    agg_vals.append((sum(vals) / len(vals)) if vals else None)
                elif re.search(r'SUM\s*\(', item, re.IGNORECASE):
                    m = re.search(r'SUM\s*\(\s*(\w+)\s*\)', item, re.IGNORECASE)
                    col = m.group(1).lower() if m else ""
                    vals = [float(r[col]) for r in rows if r.get(col) is not None]
                    agg_vals.append(sum(vals) if vals else None)
                else:
                    agg_vals.append(None)
            return [tuple(agg_vals)]

        if expr.strip() == "*":
            return [tuple(r.values()) for r in rows]

        cols = [c.strip().lower() for c in expr.split(",")]
        return [tuple(r.get(c) for c in cols) for r in rows]

    def commit(self):
        with self.lock:
            for tbl, del_set in self.uncommitted_deletions.items():
                if del_set and tbl in self.committed_tables:
                    self.committed_tables[tbl] = [r for r in self.committed_tables[tbl] if id(r) not in del_set]
                del_set.clear()
            for tbl, rows in self.uncommitted_tables.items():
                if tbl not in self.committed_tables:
                    self.committed_tables[tbl] = []
                self.committed_tables[tbl].extend(rows)
                rows.clear()

    def rollback(self):
        with self.lock:
            for rows in self.uncommitted_tables.values():
                rows.clear()
            for del_set in self.uncommitted_deletions.values():
                del_set.clear()

    def cursor(self):
        return MockDbCursor(self)

    def close(self):
        pass


class MockStompClient:
    """Mock STOMP WebSocket client simulating live subscription pushes."""
    def __init__(self):
        self.connected = False
        self.subscriptions: Dict[str, List["queue.Queue[Dict[str, Any]]"]] = {}
        self.published_events: List[Dict[str, Any]] = []

    def connect(self, host: str = "127.0.0.1", port: int = 8085, endpoint: str = "/ws") -> bool:
        self.connected = True
        return True

    def disconnect(self):
        self.connected = False
        self.subscriptions.clear()

    def subscribe(self, destination: str) -> "queue.Queue[Dict[str, Any]]":
        q: "queue.Queue[Dict[str, Any]]" = queue.Queue()
        self.subscriptions.setdefault(destination, []).append(q)
        return q

    def broadcast(self, destination: str, payload: Dict[str, Any]):
        self.published_events.append({"destination": destination, "payload": payload})
        # Exact match or prefix match
        for dest, queues in self.subscriptions.items():
            if dest == destination or destination.startswith(dest):
                for q in queues:
                    q.put(payload)


class MockHttpClient:
    """
    High-fidelity Mock HTTP Client simulating Spring Boot 3 REST endpoints,
    Rate Limiting, Security Headers, User Sessions, RBAC Role Enforcement,
    and Scale Calibration endpoints.
    """
    def __init__(self):
        self.api_keys = {"master-test-key-2026", "operator-secret-key-xyz"}
        self.sessions: Dict[str, Dict[str, Any]] = {}
        self.scale_calibrations: Dict[str, Dict[str, Any]] = {
            "70b3d57ed007985a": {"devEui": "70b3d57ed007985a", "calSlope": 420.5, "calZero": 0.125, "hopperCapacityKg": 50.0},
            "766979864b726282": {"devEui": "766979864b726282", "calSlope": 415.0, "calZero": 0.110, "hopperCapacityKg": 50.0},
            "a84041000181cafe": {"devEui": "a84041000181cafe", "calSlope": 425.0, "calZero": 0.130, "hopperCapacityKg": 50.0},
        }
        self.devices = {
            "70b3d57ed007985a": {"devEui": "70b3d57ed007985a", "name": "heltec-v3-poultry-01", "active": True},
            "766979864b726282": {"devEui": "766979864b726282", "name": "heltec-v3-poultry-02", "active": True},
            "a84041000181cafe": {"devEui": "a84041000181cafe", "name": "heltec-v3-poultry-03", "active": True},
        }
        self.telemetries: Dict[str, List[Dict[str, Any]]] = {
            "70b3d57ed007985a": [{
                "devEui": "70b3d57ed007985a",
                "seq": 1,
                "uptimeMs": 10000,
                "batteryMv": 3300.0,
                "temperature": 24.5,
                "humidity": 60.0,
                "pressure": 1013.25,
                "weightKg": 15.5,
                "grossWeightKg": 18.0,
                "voltageV": 1.65,
                "adcRaw": 2048,
                "timeOnAirMs": 61,
                "hopperPercentage": 77.5
            }],
            "766979864b726282": [{
                "devEui": "766979864b726282",
                "seq": 1,
                "uptimeMs": 10000,
                "batteryMv": 3300.0,
                "temperature": 25.0,
                "humidity": 62.0,
                "pressure": 1013.25,
                "weightKg": 18.0,
                "grossWeightKg": 20.0,
                "voltageV": 1.70,
                "adcRaw": 2100,
                "timeOnAirMs": 61,
                "hopperPercentage": 90.0
            }],
            "a84041000181cafe": [{
                "devEui": "a84041000181cafe",
                "seq": 1,
                "uptimeMs": 10000,
                "batteryMv": 3200.0,
                "temperature": 23.0,
                "humidity": 58.0,
                "pressure": 1013.25,
                "weightKg": 0.0,
                "grossWeightKg": 0.0,
                "voltageV": 0.0,
                "adcRaw": 0,
                "timeOnAirMs": 50,
                "hopperPercentage": 0.0
            }]
        }
        self.downlink_audit_log: List[Dict[str, Any]] = []
        self.request_counts: Dict[str, List[float]] = {}
        self.rate_limit_per_min = 120

    def _apply_security_headers(self, headers: Dict[str, str]) -> Dict[str, str]:
        headers["X-Content-Type-Options"] = "nosniff"
        headers["X-Frame-Options"] = "SAMEORIGIN"
        headers["X-XSS-Protection"] = "1; mode=block"
        headers["Referrer-Policy"] = "strict-origin-when-cross-origin"
        headers["Content-Type"] = "application/json"
        return headers

    def _check_rate_limit(self, client_ip: str = "127.0.0.1") -> bool:
        now = time.time()
        timestamps = self.request_counts.setdefault(client_ip, [])
        self.request_counts[client_ip] = [t for t in timestamps if now - t < 60.0]
        if len(self.request_counts[client_ip]) >= self.rate_limit_per_min:
            return False
        self.request_counts[client_ip].append(now)
        return True

    def _extract_auth(self, headers: Optional[Dict[str, str]]) -> Tuple[Optional[str], Optional[Dict[str, Any]]]:
        if not headers:
            return None, None
        h_lower = {k.lower(): v for k, v in headers.items()}

        # 1. Check X-Auth-Token
        token = h_lower.get("x-auth-token")
        if token and token in self.sessions:
            return token, self.sessions[token]

        # 2. Check X-API-Key
        api_key = h_lower.get("x-api-key")
        if api_key:
            if api_key in self.sessions:
                return api_key, self.sessions[api_key]
            if api_key in self.api_keys:
                return api_key, {"username": "apikey_user", "role": "ROLE_ADMIN", "name": "API Key Admin", "token": api_key}

        # 3. Check Authorization
        auth = h_lower.get("authorization", "")
        if auth and auth.startswith("Bearer "):
            bearer_token = auth[7:].strip()
            if bearer_token in self.sessions:
                return bearer_token, self.sessions[bearer_token]
            if bearer_token in self.api_keys:
                return bearer_token, {"username": "apikey_user", "role": "ROLE_ADMIN", "name": "API Key Admin", "token": bearer_token}

        return None, None

    def _authenticate(self, headers: Optional[Dict[str, str]]) -> bool:
        _, user = self._extract_auth(headers)
        return user is not None

    def get(self, url: str, headers: Optional[Dict[str, str]] = None, params: Optional[Dict[str, Any]] = None):
        client_ip = (headers or {}).get("X-Forwarded-For", "127.0.0.1")
        resp_headers = self._apply_security_headers({})

        if not self._check_rate_limit(client_ip):
            return MockHttpResponse(429, {"error": "Too Many Requests - Rate limit exceeded (120 req/min)"}, resp_headers)

        path = url.replace("http://127.0.0.1:8085", "").replace("http://localhost:8085", "")

        # Public endpoints
        if path == "/" or path == "/index.html":
            html_content = "<!DOCTYPE html><html><head><title>SDN Poultry SCADA</title></head><body><h1>SDN Dashboard</h1></body></html>"
            resp_headers["Content-Type"] = "text/html; charset=UTF-8"
            return MockHttpResponse(200, html_content, resp_headers)

        if path == "/api/status":
            return MockHttpResponse(200, {
                "status": "healthy",
                "mqttConnected": True,
                "database": "connected",
                "activeNodesCount": len(self.devices),
                "serverTime": "2026-11-17T18:00:00Z"
            }, resp_headers)

        if path == "/api/stats":
            return MockHttpResponse(200, {
                "totalUplinks": sum(len(h) for h in self.telemetries.values()),
                "totalDownlinks": len(self.downlink_audit_log),
                "activeDevices": len(self.devices)
            }, resp_headers)

        if path == "/api/galpones":
            latest = [hist[-1] for hist in self.telemetries.values() if hist]
            return MockHttpResponse(200, latest, resp_headers)

        if path.startswith("/api/galpones/"):
            parts = path.split("/")
            dev_eui = parts[3].lower()
            if len(parts) == 4:
                hist = self.telemetries.get(dev_eui, [])
                if not hist:
                    return MockHttpResponse(404, {"error": "Device not found or no telemetry yet"}, resp_headers)
                return MockHttpResponse(200, hist[-1], resp_headers)
            elif len(parts) == 5 and parts[4] == "history":
                hist = self.telemetries.get(dev_eui, [])
                return MockHttpResponse(200, hist, resp_headers)

        if path == "/api/sdn/downlinks":
            return MockHttpResponse(200, self.downlink_audit_log, resp_headers)

        # Scale calibration query endpoint
        if "/scale/calibration" in path:
            if not self._authenticate(headers):
                return MockHttpResponse(401, {"error": "Unauthorized", "message": "Autenticación requerida"}, resp_headers)
            parts = path.split("/")
            dev_eui = parts[3].lower() if len(parts) >= 4 else "70b3d57ed007985a"
            cal = self.scale_calibrations.get(dev_eui, {
                "devEui": dev_eui,
                "calSlope": 420.5,
                "calZero": 0.125,
                "hopperCapacityKg": 50.0
            })
            return MockHttpResponse(200, cal, resp_headers)

        # Module SCADA endpoints
        if path.startswith("/api/modules/vision/"):
            dev_eui = path.split("/")[-1]
            return MockHttpResponse(200, {
                "devEui": dev_eui,
                "eggCount": 12,
                "targetCount": 15,
                "efficiency": 80.0,
                "inferenceTimeMs": 42,
                "cameraStatus": "CONNECTED",
                "confidence": 0.75
            }, resp_headers)

        if path.startswith("/api/modules/scale/"):
            dev_eui = path.split("/")[-1]
            return MockHttpResponse(200, {
                "devEui": dev_eui,
                "netWeightKg": 15.50,
                "grossWeightKg": 15.50,
                "hopperPercentage": 77.5,
                "voltageV": 1.65,
                "adcRaw": 2048
            }, resp_headers)

        if path.startswith("/api/modules/climate/"):
            dev_eui = path.split("/")[-1]
            return MockHttpResponse(200, {
                "devEui": dev_eui,
                "temperature": 24.5,
                "humidity": 60.0,
                "pressure": 1013.25,
                "thiIndex": 68.4,
                "comfortStatus": "CONFORTABLE"
            }, resp_headers)

        if path == "/api/modules/gateway":
            return MockHttpResponse(200, {
                "model": "Laird Sentrius RG1",
                "eui": "c0ee40ffff29424b",
                "status": "ONLINE",
                "channelsActive": 8,
                "packetErrorRate": 0.15,
                "averageRssi": -65.0,
                "averageSnr": 8.5
            }, resp_headers)

        if path == "/api/modules/gateway/logs":
            return MockHttpResponse(200, [
                {"timestamp": "2026-11-17T18:00:00Z", "direction": "UPLINK", "devEui": "70b3d57ed007985a", "frequency": 902.3, "sf": "SF7", "rssi": -65, "snr": 8.5}
            ], resp_headers)

        if path == "/api/auth/me":
            token, user = self._extract_auth(headers)
            if not user:
                return MockHttpResponse(401, {"error": "Unauthorized", "message": "Sesión inválida o expirada"}, resp_headers)
            return MockHttpResponse(200, user, resp_headers)

        # Protected query endpoint: /api/keys
        if path.startswith("/api/keys"):
            token, user = self._extract_auth(headers)
            if not user:
                return MockHttpResponse(401, {"error": "No autorizado: API Key inválida o ausente"}, resp_headers)
            if user.get("role") == "ROLE_OPERADOR":
                return MockHttpResponse(403, {"error": "Forbidden", "message": "Acceso denegado: Se requiere rol ROLE_ADMIN"}, resp_headers)
            return MockHttpResponse(200, [{"id": 1, "name": "master-test-key-2026", "active": True}], resp_headers)

        return MockHttpResponse(404, {"error": "Not Found"}, resp_headers)

    def post(self, url: str, json_data: Optional[Dict[str, Any]] = None, headers: Optional[Dict[str, str]] = None):
        client_ip = (headers or {}).get("X-Forwarded-For", "127.0.0.1")
        resp_headers = self._apply_security_headers({})

        if not self._check_rate_limit(client_ip):
            return MockHttpResponse(429, {"error": "Too Many Requests - Rate limit exceeded (120 req/min)"}, resp_headers)

        path = url.replace("http://127.0.0.1:8085", "").replace("http://localhost:8085", "")

        # Auth endpoints
        if path == "/api/auth/login":
            creds = json_data or {}
            username = creds.get("username", "")
            password = creds.get("password", "")
            if username == "admin" and password == "server":
                token = f"session-admin-{int(time.time()*1000)}"
                self.sessions[token] = {
                    "username": "admin",
                    "role": "ROLE_ADMIN",
                    "name": "Julio C. (Administrador SDN)",
                    "initials": "JC",
                    "token": token
                }
                return MockHttpResponse(200, {
                    "status": "SUCCESS",
                    "token": token,
                    "username": "admin",
                    "role": "ROLE_ADMIN",
                    "name": "Julio C. (Administrador SDN)",
                    "initials": "JC",
                    "apiKey": "master-test-key-2026"
                }, resp_headers)
            elif username == "operador" and password == "operador123":
                token = f"session-operador-{int(time.time()*1000)}"
                self.sessions[token] = {
                    "username": "operador",
                    "role": "ROLE_OPERADOR",
                    "name": "Operador Galpón",
                    "initials": "OP",
                    "token": token
                }
                return MockHttpResponse(200, {
                    "status": "SUCCESS",
                    "token": token,
                    "username": "operador",
                    "role": "ROLE_OPERADOR",
                    "name": "Operador Galpón",
                    "initials": "OP"
                }, resp_headers)
            else:
                return MockHttpResponse(401, {"error": "Unauthorized", "message": "Credenciales inválidas"}, resp_headers)

        if path == "/api/auth/logout":
            token, _ = self._extract_auth(headers)
            if token and token in self.sessions:
                del self.sessions[token]
            return MockHttpResponse(200, {"status": "SUCCESS", "message": "Sesión cerrada correctamente"}, resp_headers)

        # Public preview endpoint
        if path == "/api/sdn/preview":
            req = json_data or {}
            packed = pack_sdn_command(
                req.get("cmdId", 0),
                req.get("sfTarget", 0),
                req.get("txPower", 0),
                req.get("sampleRate", 0),
                req.get("payloadMode", 0),
                req.get("sleepEnable", 0),
                req.get("paramValue", 0),
                strict_bounds=False
            )
            unpacked = unpack_sdn_command(packed)
            unpacked["hexDump"] = f"0x{unpacked['word32']:08X}"
            unpacked["base64"] = to_base64(packed)
            return MockHttpResponse(200, unpacked, resp_headers)

        # Scale endpoints
        if "/scale/tare" in path:
            token, user = self._extract_auth(headers)
            if not user:
                return MockHttpResponse(401, {"error": "Unauthorized", "message": "No autorizado: API Key o sesión ausente"}, resp_headers)
            if user.get("role") == "ROLE_OPERADOR":
                return MockHttpResponse(403, {"error": "Forbidden", "message": "Acceso denegado: Se requiere rol ROLE_ADMIN"}, resp_headers)
            parts = path.split("/")
            dev_eui = parts[3].lower() if len(parts) >= 4 else "70b3d57ed007985a"
            tare_kg = 15.50
            if dev_eui not in self.scale_calibrations:
                self.scale_calibrations[dev_eui] = {"devEui": dev_eui, "calSlope": 420.5, "calZero": 0.0, "hopperCapacityKg": 50.0}
            self.scale_calibrations[dev_eui]["calZero"] = 0.0
            audit = {
                "timestamp": "2026-11-17T18:00:00Z",
                "devEui": dev_eui,
                "action": "SCALE_TARE",
                "tareKg": tare_kg,
                "status": "SUCCESS"
            }
            self.downlink_audit_log.insert(0, audit)
            return MockHttpResponse(200, {"status": "SUCCESS", "devEui": dev_eui, "tareKg": tare_kg, "timestamp": "2026-11-17T18:00:00Z"}, resp_headers)

        if "/scale/calibrate" in path:
            token, user = self._extract_auth(headers)
            if not user:
                return MockHttpResponse(401, {"error": "Unauthorized", "message": "No autorizado: API Key o sesión ausente"}, resp_headers)
            if user.get("role") == "ROLE_OPERADOR":
                return MockHttpResponse(403, {"error": "Forbidden", "message": "Acceso denegado: Se requiere rol ROLE_ADMIN"}, resp_headers)
            parts = path.split("/")
            dev_eui = parts[3].lower() if len(parts) >= 4 else "70b3d57ed007985a"
            req = json_data or {}
            cal_slope = req.get("calSlope", 420.5)
            cal_zero = req.get("calZero", 0.125)
            known_weight = req.get("knownWeightKg", 10.0)

            if cal_slope is not None and cal_slope <= 0:
                return MockHttpResponse(400, {"error": "Pendiente de calibración inválida (debe ser positiva)"}, resp_headers)
            if known_weight is not None and (known_weight < 0 or known_weight > 50.0):
                return MockHttpResponse(400, {"error": "Peso patrón excede la capacidad de la tolva (50.0 kg)"}, resp_headers)

            self.scale_calibrations[dev_eui] = {
                "devEui": dev_eui,
                "calSlope": cal_slope,
                "calZero": cal_zero,
                "hopperCapacityKg": 50.0
            }
            audit = {
                "timestamp": "2026-11-17T18:00:00Z",
                "devEui": dev_eui,
                "action": "SCALE_CALIBRATION",
                "calSlope": cal_slope,
                "calZero": cal_zero,
                "status": "SUCCESS"
            }
            self.downlink_audit_log.insert(0, audit)
            return MockHttpResponse(200, {"status": "SUCCESS", "devEui": dev_eui, "calSlope": cal_slope, "calZero": cal_zero}, resp_headers)

        # Protected mutating endpoints
        if path == "/api/sdn/downlink":
            token, user = self._extract_auth(headers)
            if not user:
                return MockHttpResponse(401, {"error": "No autorizado: API Key inválida o ausente"}, resp_headers)
            if user.get("role") == "ROLE_OPERADOR":
                return MockHttpResponse(403, {"error": "Forbidden", "message": "Acceso denegado: Se requiere rol ROLE_ADMIN"}, resp_headers)
            req = json_data or {}
            dev_eui = req.get("devEui", "").strip().lower()
            if not dev_eui:
                return MockHttpResponse(400, {"error": "DevEUI es obligatorio"}, resp_headers)

            packed = pack_sdn_command(
                req.get("cmdId", 0),
                req.get("sfTarget", 0),
                req.get("txPower", 0),
                req.get("sampleRate", 0),
                req.get("payloadMode", 0),
                req.get("sleepEnable", 0),
                req.get("paramValue", 0),
                strict_bounds=False
            )
            audit = {
                "timestamp": "2026-11-17T18:00:00Z",
                "devEui": dev_eui,
                "cmdId": req.get("cmdId", 0),
                "base64": to_base64(packed),
                "status": "ENQUEUED_IN_CHIRPSTACK"
            }
            self.downlink_audit_log.insert(0, audit)
            return MockHttpResponse(200, audit, resp_headers)

        if path.startswith("/api/devices"):
            token, user = self._extract_auth(headers)
            if not user:
                return MockHttpResponse(401, {"error": "No autorizado: API Key inválida o ausente"}, resp_headers)
            if user.get("role") == "ROLE_OPERADOR":
                return MockHttpResponse(403, {"error": "Forbidden", "message": "Acceso denegado: Se requiere rol ROLE_ADMIN"}, resp_headers)
            dev = json_data or {}
            dev_eui = dev.get("devEui", "").strip().lower()
            if not dev_eui:
                return MockHttpResponse(400, {"error": "devEui required"}, resp_headers)
            self.devices[dev_eui] = dev
            return MockHttpResponse(201, dev, resp_headers)

        if path.startswith("/api/keys"):
            token, user = self._extract_auth(headers)
            if not user:
                return MockHttpResponse(401, {"error": "No autorizado: API Key inválida o ausente"}, resp_headers)
            if user.get("role") == "ROLE_OPERADOR":
                return MockHttpResponse(403, {"error": "Forbidden", "message": "Acceso denegado: Se requiere rol ROLE_ADMIN"}, resp_headers)
            k = (json_data or {}).get("key")
            if k:
                self.api_keys.add(k)
            return MockHttpResponse(201, {"key": k, "status": "created"}, resp_headers)

        return MockHttpResponse(404, {"error": "Not Found"}, resp_headers)

    def delete(self, url: str, headers: Optional[Dict[str, str]] = None):
        client_ip = (headers or {}).get("X-Forwarded-For", "127.0.0.1")
        resp_headers = self._apply_security_headers({})

        if not self._check_rate_limit(client_ip):
            return MockHttpResponse(429, {"error": "Too Many Requests - Rate limit exceeded (120 req/min)"}, resp_headers)

        token, user = self._extract_auth(headers)
        if not user:
            return MockHttpResponse(401, {"error": "No autorizado: API Key inválida o ausente"}, resp_headers)
        if user.get("role") == "ROLE_OPERADOR":
            return MockHttpResponse(403, {"error": "Forbidden", "message": "Acceso denegado: Se requiere rol ROLE_ADMIN"}, resp_headers)

        path = url.replace("http://127.0.0.1:8085", "").replace("http://localhost:8085", "")
        if path.startswith("/api/devices/"):
            dev_eui = path.split("/")[3].lower()
            self.devices.pop(dev_eui, None)
            return MockHttpResponse(204, {}, resp_headers)

        return MockHttpResponse(404, {"error": "Not Found"}, resp_headers)


class MockHttpResponse:
    def __init__(self, status_code: int, content: Union[Dict[str, Any], List[Any], str], headers: Dict[str, str]):
        self.status_code = status_code
        self._content = content
        self.headers = headers

    @property
    def text(self) -> str:
        if isinstance(self._content, str):
            return self._content
        return json.dumps(self._content)

    def json(self) -> Any:
        if isinstance(self._content, (dict, list)):
            return self._content
        return json.loads(self._content)


class MockClaudeIpcServer:
    """Mock AF_UNIX stream socket server simulating Claude Opus IPC protocol."""
    def __init__(self, token: str = "mock-secret-peer-token-2026"):
        self.token = token
        self.sock_path = os.path.join(tempfile.gettempdir(), f"mock_claude_{os.getpid()}_{time.time_ns()}.sock")
        self.server_sock: Optional[socket.socket] = None
        self.running = False
        self._thread: Optional[threading.Thread] = None
        self.authenticated_sessions = 0
        self.requests_handled = 0

    def start(self):
        self.running = True
        self.server_sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.server_sock.bind(self.sock_path)
        self.server_sock.listen(5)
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def _run(self):
        while self.running:
            try:
                conn, _ = self.server_sock.accept()
                threading.Thread(target=self._handle_client, args=(conn,), daemon=True).start()
            except OSError:
                break

    def _handle_client(self, conn: socket.socket):
        conn.settimeout(3.0)  # 3000ms auth handshake deadline
        try:
            buffer = ""
            # Step 1: Wait for auth handshake
            while True:
                while "\n" not in buffer:
                    chunk = conn.recv(1024).decode("utf-8")
                    if not chunk:
                        conn.close()
                        return
                    buffer += chunk

                line, buffer = buffer.split("\n", 1)
                line = line.strip()
                if line:
                    break

            auth_frame = json.loads(line)
            if auth_frame.get("type") != "auth" or auth_frame.get("token") != self.token:
                conn.sendall(json.dumps({"type": "error", "message": "Invalid authentication token"}).encode("utf-8") + b"\n")
                conn.close()
                return

            self.authenticated_sessions += 1
            conn.sendall(json.dumps({"type": "auth_ok", "status": "authenticated"}).encode("utf-8") + b"\n")

            # Step 2: Request handling loop
            while self.running:
                while "\n" not in buffer:
                    chunk = conn.recv(4096).decode("utf-8")
                    if not chunk:
                        conn.close()
                        return
                    buffer += chunk

                line, buffer = buffer.split("\n", 1)
                req = json.loads(line)
                self.requests_handled += 1
                if req.get("type") == "user":
                    prompt = req.get("message", {}).get("content", "")
                    response_frame = {
                        "type": "assistant",
                        "message": {
                            "role": "assistant",
                            "content": f"# Technical Documentation\nGenerated documentation for request: {prompt[:50]}...\nSDN Controller v1.0.0."
                        }
                    }
                    conn.sendall(json.dumps(response_frame).encode("utf-8") + b"\n")

        except (socket.timeout, json.JSONDecodeError, OSError):
            pass
        finally:
            conn.close()

    def stop(self):
        self.running = False
        if self.server_sock:
            try:
                self.server_sock.close()
            except OSError:
                pass
        if os.path.exists(self.sock_path):
            try:
                os.unlink(self.sock_path)
            except OSError:
                pass


# --- Pytest Fixtures ---

@pytest.fixture(scope="session")
def is_live_postgres() -> bool:
    """True if PostgreSQL port 5432 is reachable."""
    return is_port_open("localhost", 5432)


@pytest.fixture(scope="session")
def is_live_mqtt() -> bool:
    """True if Mosquitto port 1883 is reachable."""
    return is_port_open("localhost", 1883)


@pytest.fixture(scope="session")
def is_live_spring_boot() -> bool:
    """True if Spring Boot port 8085 is reachable."""
    return is_port_open("localhost", 8085)


@pytest.fixture(scope="session")
def is_live_claude_socket() -> bool:
    """True if Claude Unix domain socket is reachable."""
    return is_unix_socket_open("/run/user/1000/cc-socks/2894.sock")


@pytest.fixture
def db_conn(is_live_postgres):
    """
    Dual-mode Database Connection fixture.
    Connects to live PostgreSQL if running, otherwise uses MockDbConnection.
    """
    if is_live_postgres:
        try:
            conn = psycopg2.connect(
                host="localhost",
                port=5432,
                user="chirpstack",
                password="chirpstack",
                dbname="chirpstack",
                connect_timeout=3
            )
            yield conn
            conn.close()
            return
        except Exception:
            pass

    mock_db = MockDbConnection()
    yield mock_db
    mock_db.close()


@pytest.fixture
def mqtt_client(is_live_mqtt):
    """
    Dual-mode MQTT Client fixture.
    """
    if is_live_mqtt:
        import paho.mqtt.client as paho_mqtt
        client = paho_mqtt.Client(client_id=f"test-runner-{os.getpid()}-{time.time_ns()}")
        try:
            client.connect("127.0.0.1", 1883, 60)
            client.loop_start()
            for _ in range(50):
                if client.is_connected():
                    break
                time.sleep(0.02)
            yield client
            client.loop_stop()
            client.disconnect()
            return
        except Exception:
            pass

    mock_client = MockMqttClient()
    mock_client.connect()
    yield mock_client
    mock_client.disconnect()


@pytest.fixture
def http_client(is_live_spring_boot):
    """
    Dual-mode REST HTTP Client fixture.
    """
    if is_live_spring_boot:
        session = requests.Session()
        session.headers.update({"Accept": "application/json"})
        # Wrapper to treat relative paths as http://127.0.0.1:8085
        class LiveClient:
            def get(self, url, **kwargs):
                u = url if url.startswith("http") else f"http://127.0.0.1:8085{url}"
                return session.get(u, **kwargs)
            def post(self, url, json_data=None, **kwargs):
                u = url if url.startswith("http") else f"http://127.0.0.1:8085{url}"
                return session.post(u, json=json_data, **kwargs)
            def delete(self, url, **kwargs):
                u = url if url.startswith("http") else f"http://127.0.0.1:8085{url}"
                return session.delete(u, **kwargs)
        yield LiveClient()
        session.close()
    else:
        yield MockHttpClient()


@pytest.fixture
def stomp_client():
    """STOMP WebSocket test client fixture."""
    client = MockStompClient()
    client.connect()
    yield client
    client.disconnect()


@pytest.fixture
def mock_claude_server():
    """High-fidelity Mock Claude Opus IPC socket server fixture."""
    server = MockClaudeIpcServer()
    server.start()
    yield server
    server.stop()
