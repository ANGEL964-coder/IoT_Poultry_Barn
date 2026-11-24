#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include <Arduino.h>

const char PAGE_INDEX[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Sistema de Pesaje Avicola - Heltec LoRa32 V3</title>
    <style>
        :root {
            --bg-base: #0a0f1d;
            --bg-card: #131c31;
            --bg-card-sub: #0c1427;
            --border: #263554;
            --border-focus: #0284c7;
            --primary: #38bdf8;
            --primary-hover: #0284c7;
            --accent: #f59e0b;
            --success: #10b981;
            --danger: #ef4444;
            --text-main: #f1f5f9;
            --text-muted: #94a3b8;
            --font-mono: "Consolas", "Courier New", monospace;
        }

        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
        }

        body {
            background-color: var(--bg-base);
            color: var(--text-main);
            padding: 16px;
            min-height: 100vh;
        }

        .icon {
            display: inline-block;
            width: 18px;
            height: 18px;
            vertical-align: middle;
            stroke-width: 2;
            stroke: currentColor;
            fill: none;
            stroke-linecap: round;
            stroke-linejoin: round;
        }

        .header {
            background: linear-gradient(135deg, #131c31 0%, #0c1427 100%);
            border: 1px solid var(--border);
            border-radius: 8px;
            padding: 16px 20px;
            margin-bottom: 16px;
            display: flex;
            justify-content: space-between;
            align-items: center;
            flex-wrap: wrap;
            gap: 12px;
        }

        .header-title h1 {
            font-size: 1.3rem;
            color: var(--primary);
            font-weight: 700;
            letter-spacing: 0.5px;
            display: flex;
            align-items: center;
            gap: 10px;
        }

        .header-title p {
            font-size: 0.82rem;
            color: var(--text-muted);
            margin-top: 3px;
        }

        .badges {
            display: flex;
            gap: 8px;
            flex-wrap: wrap;
        }

        .badge {
            font-size: 0.75rem;
            padding: 5px 10px;
            border-radius: 4px;
            font-weight: 600;
            display: flex;
            align-items: center;
            gap: 6px;
            font-family: var(--font-mono);
        }

        .badge-wifi { background: #064e3b; color: #6ee7b7; border: 1px solid #059669; }
        .badge-lora { background: #1e3a8a; color: #93c5fd; border: 1px solid #2563eb; }
        .badge-dev  { background: #1e293b; color: #cbd5e1; border: 1px solid var(--border); }
        .dot { width: 7px; height: 7px; border-radius: 50%; background: currentColor; }

        .dashboard-grid {
            display: grid;
            grid-template-columns: 1fr;
            gap: 16px;
        }

        @media (min-width: 1024px) {
            .dashboard-grid {
                grid-template-columns: 1.25fr 1fr;
            }
        }

        .card {
            background: var(--bg-card);
            border: 1px solid var(--border);
            border-radius: 8px;
            padding: 18px;
        }

        .card-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 14px;
            border-bottom: 1px solid var(--border);
            padding-bottom: 10px;
        }

        .card-title {
            font-size: 0.95rem;
            font-weight: 700;
            color: #e2e8f0;
            text-transform: uppercase;
            letter-spacing: 0.8px;
            display: flex;
            align-items: center;
            gap: 8px;
        }

        .svg-container {
            width: 100%;
            background: #080c16;
            border-radius: 6px;
            border: 1px solid var(--border);
            overflow: hidden;
        }

        svg.schematic {
            width: 100%;
            height: auto;
            display: block;
        }

        .food-box {
            background: #060913;
            border: 2px solid #0284c7;
            border-radius: 8px;
            padding: 16px;
            margin-top: 14px;
            text-align: center;
        }

        .food-box-title {
            font-size: 0.8rem;
            color: var(--primary);
            text-transform: uppercase;
            letter-spacing: 1.5px;
            font-weight: 700;
            margin-bottom: 4px;
        }

        .food-box-value {
            font-size: 3.4rem;
            font-weight: 800;
            color: #ffffff;
            font-family: var(--font-mono);
            line-height: 1.1;
        }

        .food-box-units {
            font-size: 1.3rem;
            color: var(--primary);
            margin-left: 6px;
            font-weight: 600;
        }

        .progress-bar-bg {
            background: #1e293b;
            height: 10px;
            border-radius: 5px;
            overflow: hidden;
            margin-top: 10px;
        }

        .progress-bar-fill {
            background: linear-gradient(90deg, #0284c7, #10b981);
            height: 100%;
            width: 0%;
            transition: width 0.3s ease;
        }

        .food-submetrics {
            display: flex;
            justify-content: space-around;
            margin-top: 12px;
            padding-top: 10px;
            border-top: 1px solid var(--border);
            font-size: 0.85rem;
            color: var(--text-muted);
            font-family: var(--font-mono);
        }

        .food-submetrics strong {
            color: #ffffff;
        }

        .btn {
            background: #1e293b;
            color: #f1f5f9;
            border: 1px solid var(--border);
            padding: 9px 14px;
            border-radius: 6px;
            font-size: 0.85rem;
            font-weight: 600;
            cursor: pointer;
            display: inline-flex;
            align-items: center;
            justify-content: center;
            gap: 8px;
            transition: all 0.15s ease;
        }

        .btn:hover { background: #263554; border-color: #3b82f6; }
        .btn-primary { background: #0284c7; border-color: #0369a1; }
        .btn-primary:hover { background: #0369a1; }
        .btn-success { background: #059669; border-color: #047857; }
        .btn-success:hover { background: #047857; }
        .btn-danger { background: #b91c1c; border-color: #991b1b; }
        .btn-danger:hover { background: #991b1b; }

        .btn-lora-tx {
            width: 100%;
            padding: 13px 20px;
            font-size: 0.95rem;
            font-weight: 700;
            background: #0284c7;
            border: 1px solid #38bdf8;
            border-radius: 6px;
            color: white;
            cursor: pointer;
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 10px;
            margin-bottom: 8px;
            transition: all 0.15s ease;
        }

        .btn-lora-tx:hover {
            background: #0369a1;
        }

        .btn-lora-tx:disabled {
            background: #334155;
            border-color: #475569;
            color: #94a3b8;
            cursor: not-allowed;
        }

        .btn-cancel {
            width: 100%;
            padding: 10px 16px;
            font-size: 0.85rem;
            font-weight: 700;
            background: #7f1d1d;
            border: 1px solid #b91c1c;
            border-radius: 6px;
            color: white;
            cursor: pointer;
            display: none;
            align-items: center;
            justify-content: center;
            gap: 8px;
            margin-bottom: 10px;
            transition: all 0.15s ease;
        }

        .btn-cancel:hover {
            background: #991b1b;
        }

        .tx-status-banner {
            padding: 10px 14px;
            border-radius: 6px;
            font-size: 0.85rem;
            font-family: var(--font-mono);
            margin-bottom: 12px;
            display: flex;
            align-items: center;
            gap: 8px;
        }

        .tx-status-banner.idle { background: #0c1427; border: 1px solid var(--border); color: var(--text-muted); }
        .tx-status-banner.transmitting { background: #451a03; border: 1px solid #b45309; color: #fde68a; }
        .tx-status-banner.success { background: #064e3b; border: 1px solid #059669; color: #a7f3d0; }
        .tx-status-banner.error { background: #450a0a; border: 1px solid #b91c1c; color: #fecaca; }

        .stats-grid {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 8px;
        }

        @media (min-width: 600px) {
            .stats-grid {
                grid-template-columns: repeat(3, 1fr);
            }
        }

        .stat-box {
            background: #0c1427;
            border: 1px solid var(--border);
            border-radius: 6px;
            padding: 10px;
        }

        .stat-label {
            font-size: 0.72rem;
            color: var(--text-muted);
            text-transform: uppercase;
            font-weight: 600;
            letter-spacing: 0.5px;
        }

        .stat-value {
            font-size: 1.15rem;
            font-weight: 700;
            color: var(--primary);
            font-family: var(--font-mono);
            margin-top: 3px;
        }

        .stat-value-highlight {
            color: #34d399;
        }

        .lora-controls-bar {
            background: #0c1427;
            border: 1px solid var(--border);
            border-radius: 6px;
            padding: 12px;
            margin-top: 12px;
        }

        .control-row {
            display: flex;
            gap: 8px;
            align-items: center;
            flex-wrap: wrap;
            margin-bottom: 8px;
        }

        .control-row:last-child {
            margin-bottom: 0;
        }

        .control-row label {
            font-size: 0.8rem;
            color: var(--text-muted);
            min-width: 110px;
            font-weight: 600;
        }

        .control-row select, .control-row input {
            flex: 1;
            background: #131c31;
            border: 1px solid var(--border);
            color: white;
            padding: 7px 10px;
            border-radius: 4px;
            font-size: 0.85rem;
            font-family: var(--font-mono);
        }

        .control-row select:focus, .control-row input:focus {
            outline: none;
            border-color: var(--border-focus);
        }

        .cal-monitor {
            background: #060913;
            border: 1px solid #0284c7;
            border-radius: 6px;
            padding: 12px;
            margin-bottom: 12px;
        }

        .cal-monitor-grid {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 8px;
            text-align: center;
            margin-top: 8px;
        }

        .cal-monitor-grid div span {
            display: block;
            font-size: 1.1rem;
            font-weight: 700;
            color: var(--primary);
            font-family: var(--font-mono);
            margin-top: 2px;
        }

        .cal-section {
            background: #0c1427;
            border: 1px solid var(--border);
            border-radius: 6px;
            padding: 12px;
            margin-bottom: 10px;
        }

        .cal-section-title {
            font-size: 0.82rem;
            font-weight: 700;
            color: var(--primary);
            text-transform: uppercase;
            letter-spacing: 0.5px;
            margin-bottom: 10px;
            display: flex;
            align-items: center;
            gap: 6px;
        }

        .input-group {
            display: flex;
            gap: 8px;
            margin-bottom: 8px;
        }

        .input-group input {
            flex: 1;
            background: #131c31;
            border: 1px solid var(--border);
            color: white;
            padding: 8px 10px;
            border-radius: 4px;
            font-size: 0.85rem;
            font-family: var(--font-mono);
        }

        .input-group input:focus {
            outline: none;
            border-color: var(--border-focus);
        }

        .actions-grid {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 8px;
            margin-top: 10px;
        }

        .footer {
            text-align: center;
            font-size: 0.75rem;
            color: var(--text-muted);
            margin-top: 24px;
            font-family: var(--font-mono);
        }
    </style>
</head>
<body>

    <!-- Cabecera Principal -->
    <div class="header">
        <div class="header-title">
            <h1>
                <svg class="icon" viewBox="0 0 24 24"><path d="m16 16 3-8 3 8c-.87.65-1.92 1-3 1s-2.13-.35-3-1Z"/><path d="m2 16 3-8 3 8c-.87.65-1.92 1-3 1s-2.13-.35-3-1Z"/><path d="M7 21h10"/><path d="M12 3v18"/><path d="M3 7h18"/></svg>
                Sistema de Pesaje de Alimento Avicola
            </h1>
            <p>Monitoreo de Galpon | Heltec WiFi LoRa 32 V3 (ESP32-S3 + Semtech SX1262)</p>
        </div>
        <div class="badges">
            <div class="badge badge-wifi">
                <span class="dot"></span>
                WiFi AP: Balanza-Avicola
            </div>
            <div class="badge badge-lora" id="lora-badge">
                <span class="dot"></span>
                LoRaWAN: US915 SB2
            </div>
            <div class="badge badge-dev">
                ADC0: GPIO 1 (0-3.3V)
            </div>
        </div>
    </div>

    <!-- Grid Principal -->
    <div class="dashboard-grid">

        <!-- Columna Izquierda: Esquematico SVG y Recuadro de Comida -->
        <div class="card">
            <div class="card-header">
                <div class="card-title">
                    <svg class="icon" viewBox="0 0 24 24"><polyline points="22 12 18 12 15 21 9 3 6 12 2 12"/></svg>
                    Tolva de Pesaje Avicola (Esquematico SVG)
                </div>
                <span id="load-status-badge" style="font-size:0.75rem; background:#064e3b; color:#6ee7b7; padding:2px 8px; border-radius:4px; font-family:var(--font-mono); font-weight:700;">ESTABLE</span>
            </div>

            <!-- Esquematico SVG CAD -->
            <div class="svg-container">
                <svg class="schematic" viewBox="0 0 600 420" xmlns="http://www.w3.org/2000/svg">
                    <defs>
                        <linearGradient id="vigaGrad" x1="0%" y1="0%" x2="100%" y2="0%">
                            <stop offset="0%" stop-color="#334155" />
                            <stop offset="50%" stop-color="#64748b" />
                            <stop offset="100%" stop-color="#334155" />
                        </linearGradient>

                        <linearGradient id="hopperGrad" x1="0%" y1="0%" x2="100%" y2="0%">
                            <stop offset="0%" stop-color="#1e293b" />
                            <stop offset="30%" stop-color="#475569" />
                            <stop offset="70%" stop-color="#64748b" />
                            <stop offset="100%" stop-color="#0f172a" />
                        </linearGradient>

                        <linearGradient id="feedGrad" x1="0%" y1="0%" x2="0%" y2="100%">
                            <stop offset="0%" stop-color="#f59e0b" />
                            <stop offset="60%" stop-color="#d97706" />
                            <stop offset="100%" stop-color="#92400e" />
                        </linearGradient>

                        <clipPath id="hopperClip">
                            <polygon points="190,130 410,130 360,270 240,270" />
                        </clipPath>
                    </defs>

                    <!-- Viga Superior Estructural Galpon -->
                    <rect x="40" y="20" width="520" height="18" fill="url(#vigaGrad)" stroke="#1e293b" stroke-width="1" />
                    <line x1="80" y1="20" x2="80" y2="38" stroke="#0f172a" stroke-width="2" />
                    <line x1="200" y1="20" x2="200" y2="38" stroke="#0f172a" stroke-width="2" />
                    <line x1="400" y1="20" x2="400" y2="38" stroke="#0f172a" stroke-width="2" />
                    <line x1="520" y1="20" x2="520" y2="38" stroke="#0f172a" stroke-width="2" />

                    <!-- Grillete y Cable de Traccion -->
                    <line x1="300" y1="38" x2="300" y2="65" stroke="#94a3b8" stroke-width="3" />
                    <circle cx="300" cy="42" r="3" fill="#cbd5e1" />

                    <!-- Celda de Carga / Transductor -->
                    <rect x="282" y="65" width="36" height="28" rx="2" fill="#0284c7" stroke="#38bdf8" stroke-width="1.5" />
                    <text x="300" y="82" fill="#ffffff" font-size="8" font-family="Consolas, monospace" font-weight="bold" text-anchor="middle">S-CELL</text>

                    <!-- Cable de Señal al Acondicionador ADC0 -->
                    <path d="M 318 78 Q 370 75 420 85" fill="none" stroke="#38bdf8" stroke-width="1.5" stroke-dasharray="3,2" />
                    <rect x="420" y="70" width="140" height="32" rx="4" fill="#0c1427" stroke="#0284c7" stroke-width="1.2" />
                    <text x="490" y="84" fill="#94a3b8" font-size="8" font-family="Consolas, monospace" font-weight="bold" text-anchor="middle">ACONDICIONADOR ADC0</text>
                    <text x="490" y="96" fill="#38bdf8" font-size="8.5" font-family="Consolas, monospace" text-anchor="middle" id="svg-volt-pill">0.000 V | GPIO 1</text>

                    <!-- Tirantes de Suspension de Tolva -->
                    <line x1="300" y1="93" x2="210" y2="125" stroke="#64748b" stroke-width="2.5" />
                    <line x1="300" y1="93" x2="390" y2="125" stroke="#64748b" stroke-width="2.5" />

                    <!-- Cuerpo Conico de Tolva -->
                    <polygon points="180,125 420,125 365,275 235,275" fill="url(#hopperGrad)" stroke="#334155" stroke-width="1.5" />
                    <ellipse cx="300" cy="125" rx="120" ry="10" fill="#334155" stroke="#64748b" stroke-width="1.5" />

                    <!-- Nivel Dinamico de Alimento -->
                    <g clip-path="url(#hopperClip)">
                        <rect x="180" y="130" width="240" height="150" fill="#0c1427" opacity="0.7" />
                        <rect id="svg-feed-rect" x="180" y="270" width="240" height="0" fill="url(#feedGrad)" opacity="0.95" />
                        <ellipse id="svg-feed-top" cx="300" cy="270" rx="60" ry="5" fill="#fde68a" />
                    </g>

                    <!-- Mirilla Frontal con Escala Metrica -->
                    <rect x="292" y="145" width="16" height="115" fill="rgba(255,255,255,0.08)" stroke="#64748b" stroke-width="1" />
                    <line x1="292" y1="155" x2="299" y2="155" stroke="#94a3b8" stroke-width="1" />
                    <line x1="292" y1="175" x2="304" y2="175" stroke="#94a3b8" stroke-width="1.5" />
                    <line x1="292" y1="195" x2="299" y2="195" stroke="#94a3b8" stroke-width="1" />
                    <line x1="292" y1="215" x2="304" y2="215" stroke="#94a3b8" stroke-width="1.5" />
                    <line x1="292" y1="235" x2="299" y2="235" stroke="#94a3b8" stroke-width="1" />
                    <line x1="292" y1="255" x2="304" y2="255" stroke="#94a3b8" stroke-width="1.5" />

                    <!-- Compuerta / Valvula de Descarga -->
                    <rect x="265" y="275" width="70" height="16" fill="#1e293b" stroke="#475569" stroke-width="1.5" rx="1" />
                    <line x1="335" y1="283" x2="355" y2="283" stroke="#f59e0b" stroke-width="3" />
                    <circle cx="357" cy="283" r="4" fill="#f59e0b" />

                    <!-- Tubo de Caida de Alimento -->
                    <rect x="286" y="291" width="28" height="35" fill="#334155" stroke="#1e293b" />

                    <!-- Plato Comedero Circular Avicola -->
                    <ellipse cx="300" cy="360" rx="150" ry="22" fill="#0c1427" stroke="#334155" stroke-width="2" />
                    <ellipse cx="300" cy="358" rx="130" ry="17" fill="#1e293b" stroke="#f59e0b" stroke-width="1" />
                    <ellipse cx="300" cy="357" rx="115" ry="13" fill="#d97706" opacity="0.8" />
                </svg>
            </div>

            <!-- Recuadro Central Destacado de Comida -->
            <div class="food-box">
                <div class="food-box-title">Cantidad de Comida en Tolva</div>
                <div class="food-box-value">
                    <span id="weight-kg">0.00</span><span class="food-box-units">kg</span>
                </div>
                <div class="progress-bar-bg">
                    <div class="progress-bar-fill" id="hopper-progress"></div>
                </div>
                <div class="food-submetrics">
                    <div>Libras: <strong id="weight-lbs">0.00 lbs</strong></div>
                    <div>Capacidad: <strong id="hopper-pct">0.0%</strong></div>
                    <div>Peso Bruto: <strong id="gross-kg">0.00 kg</strong></div>
                </div>
            </div>
        </div>

        <!-- Columna Derecha: LoRaWAN Telemetria, Conexion y Calibracion -->
        <div style="display:flex; flex-direction:column; gap:16px;">

            <!-- Tarjeta 1: Control de Conexion y Telemetria LoRaWAN -->
            <div class="card">
                <div class="card-header">
                    <div class="card-title">
                        <svg class="icon" viewBox="0 0 24 24"><path d="M4.9 19.1C1 15.2 1 8.8 4.9 4.9"/><path d="M7.8 16.2c-2.3-2.3-2.3-6.1 0-8.5"/><circle cx="12" cy="12" r="2"/><path d="M16.2 7.8c2.3 2.3 2.3 6.1 0 8.5"/><path d="M19.1 4.9C23 8.8 23 15.1 19.1 19"/></svg>
                        Conexion y Telemetria LoRaWAN
                    </div>
                    <span id="lora-state-badge" style="font-size:0.75rem; background:#064e3b; color:#6ee7b7; padding:2px 8px; border-radius:4px; font-family:var(--font-mono); font-weight:700;">UNIDO</span>
                </div>

                <!-- Boton Principal de Envio Manual -->
                <button class="btn-lora-tx" id="btn-tx" onclick="triggerLoraTx()">
                    <svg class="icon" viewBox="0 0 24 24"><line x1="22" y1="2" x2="11" y2="13"/><polygon points="22 2 15 22 11 13 2 9 22 2"/></svg>
                    <span id="btn-tx-text">TRANSMITIR DATOS AHORA (UPLINK 204B)</span>
                </button>

                <!-- Boton de Cancelacion de Transmision (visible durante transmision) -->
                <button class="btn-cancel" id="btn-cancel-tx" onclick="cancelLoraTx(false, 'Transmision cancelada manualmente por el operador.')">
                    <svg class="icon" viewBox="0 0 24 24"><line x1="18" y1="6" x2="6" y2="18"/><line x1="6" y1="6" x2="18" y2="18"/></svg>
                    CANCELAR TRANSMISION Y RESTABLECER BOTON
                </button>

                <!-- Banner de Estado de Transmision -->
                <div id="tx-banner" class="tx-status-banner idle">
                    <svg class="icon" viewBox="0 0 24 24"><circle cx="12" cy="12" r="10"/><line x1="12" y1="16" x2="12" y2="12"/><line x1="12" y1="8" x2="12.01" y2="8"/></svg>
                    <span id="tx-banner-text">Listo para transmitir paquete a ChirpStack</span>
                </div>

                <!-- Grid de Metricas de Red y Time on Air -->
                <div class="stats-grid">
                    <div class="stat-box">
                        <div class="stat-label">Tiempo en Aire (ToA)</div>
                        <div class="stat-value stat-value-highlight"><span id="toa-ms">0.0</span> ms</div>
                    </div>
                    <div class="stat-box">
                        <div class="stat-label">Ventana TX+RX</div>
                        <div class="stat-value"><span id="tx-duration-ms">0</span> ms</div>
                    </div>
                    <div class="stat-box">
                        <div class="stat-label">DevAddr (OTAA)</div>
                        <div class="stat-value" id="dev-addr">0x00000000</div>
                    </div>
                    <div class="stat-box">
                        <div class="stat-label">Contador Frame (FCnt)</div>
                        <div class="stat-value">#<span id="uplink-seq">0</span></div>
                    </div>
                    <div class="stat-box">
                        <div class="stat-label">Datarate / SF</div>
                        <div class="stat-value">DR<span id="dr-val">3</span> (SF<span id="sf-val">7</span>)</div>
                    </div>
                    <div class="stat-box">
                        <div class="stat-label">Potencia TX</div>
                        <div class="stat-value"><span id="txp-val">14</span> dBm</div>
                    </div>
                    <div class="stat-box">
                        <div class="stat-label">RSSI Ultimo Downlink</div>
                        <div class="stat-value"><span id="rssi-val">--</span> dBm</div>
                    </div>
                    <div class="stat-box">
                        <div class="stat-label">SNR Ultimo Downlink</div>
                        <div class="stat-value"><span id="snr-val">--</span> dB</div>
                    </div>
                    <div class="stat-box">
                        <div class="stat-label">Ultimo Comando SDN</div>
                        <div class="stat-value" style="font-size:0.9rem; color:#fde68a;" id="sdn-cmd">NINGUNO</div>
                    </div>
                </div>

                <!-- Controles de Parametros LoRaWAN -->
                <div class="lora-controls-bar">
                    <div class="control-row">
                        <label>Datarate (DR):</label>
                        <select id="sel-dr" onchange="updateLoraConfig()">
                            <option value="3">DR3 - SF7 BW125 (Por Defecto)</option>
                            <option value="2">DR2 - SF8 BW125</option>
                            <option value="1">DR1 - SF9 BW125</option>
                            <option value="0">DR0 - SF10 BW125</option>
                            <option value="4">DR4 - SF8 BW500</option>
                        </select>
                    </div>
                    <div class="control-row">
                        <label>Control ADR:</label>
                        <select id="sel-adr" onchange="updateLoraConfig()">
                            <option value="0">Desactivado (Manual)</option>
                            <option value="1">Activado (Adaptativo)</option>
                        </select>
                    </div>
                    <div class="control-row">
                        <label>Tipo Uplink:</label>
                        <select id="sel-confirmed" onchange="updateLoraConfig()">
                            <option value="0">No Confirmado (Unconfirmed)</option>
                            <option value="1">Confirmado (Requiere ACK)</option>
                        </select>
                    </div>
                    <div class="control-row" style="margin-top:10px;">
                        <button class="btn btn-primary" style="flex:1;" onclick="rejoinOtaa()">
                            <svg class="icon" viewBox="0 0 24 24"><polyline points="23 4 23 10 17 10"/><polyline points="1 20 1 14 7 14"/><path d="M3.51 9a9 9 0 0 1 14.85-3.36L23 10M1 14l4.64 4.36A9 9 0 0 0 20.49 15"/></svg>
                            Reconectar / Rejoin OTAA
                        </button>
                    </div>
                </div>
            </div>

            <!-- Tarjeta 2: Calibracion de Celda de Carga (ADC0 GPIO 1) -->
            <div class="card">
                <div class="card-header">
                    <div class="card-title">
                        <svg class="icon" viewBox="0 0 24 24"><circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z"/></svg>
                        Calibracion de Celda de Carga
                    </div>
                    <button class="btn btn-danger" style="font-size:0.75rem; padding:3px 8px;" onclick="resetCalibration()">Reset Fabrica</button>
                </div>

                <!-- Monitor en Vivo del Objeto en Estado de Carga -->
                <div class="cal-monitor">
                    <div style="font-size:0.75rem; font-weight:700; color:#94a3b8; text-transform:uppercase; letter-spacing:0.5px;">Monitor de Carga Actual:</div>
                    <div class="cal-monitor-grid">
                        <div>
                            <div style="font-size:0.7rem; color:#64748b;">VOLTAJE ADC0</div>
                            <span id="cal-live-v">0.000 V</span>
                        </div>
                        <div>
                            <div style="font-size:0.7rem; color:#64748b;">CUENTAS RAW</div>
                            <span id="cal-live-raw">0</span>
                        </div>
                        <div>
                            <div style="font-size:0.7rem; color:#64748b;">PESO CALCULADO</div>
                            <span id="cal-live-kg">0.00 kg</span>
                        </div>
                    </div>
                </div>

                <!-- Calibracion Automatica Zero y Span -->
                <div class="cal-section">
                    <div class="cal-section-title">
                        <svg class="icon" viewBox="0 0 24 24"><path d="m16 16 3-8 3 8c-.87.65-1.92 1-3 1s-2.13-.35-3-1Z"/><path d="m2 16 3-8 3 8c-.87.65-1.92 1-3 1s-2.13-.35-3-1Z"/><path d="M7 21h10"/><path d="M12 3v18"/><path d="M3 7h18"/></svg>
                        1. Calibracion Automatica (Cero y Span)
                    </div>
                    <div style="margin-bottom:8px;">
                        <button class="btn btn-primary" style="width:100%;" onclick="setZero()">
                            Paso 1: Fijar Cero (Tolva Vacia)
                        </button>
                    </div>
                    <div class="input-group">
                        <input type="number" id="span-weight" step="0.1" value="10.0" placeholder="Masa Patron Conocida (kg)">
                        <button class="btn btn-success" onclick="setSpan()">
                            Paso 2: Fijar Span
                        </button>
                    </div>
                </div>

                <!-- Calibracion Manual Pendiente -->
                <div class="cal-section">
                    <div class="cal-section-title">
                        <svg class="icon" viewBox="0 0 24 24"><line x1="4" y1="21" x2="4" y2="14"/><line x1="4" y1="10" x2="4" y2="3"/><line x1="12" y1="21" x2="12" y2="12"/><line x1="12" y1="8" x2="12" y2="3"/><line x1="20" y1="21" x2="20" y2="16"/><line x1="20" y1="12" x2="20" y2="3"/><line x1="1" y1="14" x2="7" y2="14"/><line x1="9" y1="8" x2="15" y2="8"/><line x1="17" y1="16" x2="23" y2="16"/></svg>
                        2. Calibracion Manual (Pendiente V/kg)
                    </div>
                    <div class="input-group">
                        <input type="number" id="manual-slope" step="0.0001" placeholder="Pendiente (V/kg)">
                        <button class="btn btn-primary" onclick="setManualSlope()">Asignar Pendiente</button>
                    </div>
                    <div class="input-group">
                        <input type="number" id="manual-zero" step="0.001" placeholder="Voltaje Cero (V)">
                        <button class="btn btn-primary" onclick="setManualZero()">Asignar Cero</button>
                    </div>
                </div>

                <!-- Acciones de Memoria y Tara -->
                <div class="actions-grid">
                    <button class="btn btn-primary" onclick="setTare()">
                        <svg class="icon" viewBox="0 0 24 24"><circle cx="12" cy="12" r="10"/><line x1="22" y1="12" x2="18" y2="12"/><line x1="6" y1="12" x2="2" y2="12"/><line x1="12" y1="6" x2="12" y2="2"/><line x1="12" y1="22" x2="12" y2="18"/></svg>
                        Tara
                    </button>
                    <button class="btn btn-success" onclick="saveToNvs()">
                        <svg class="icon" viewBox="0 0 24 24"><path d="M19 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v11a2 2 0 0 1-2 2z"/><polyline points="17 21 17 13 7 13 7 21"/><polyline points="7 3 7 8 15 8"/></svg>
                        Guardar NVS
                    </button>
                    <button class="btn" onclick="clearTare()">
                        Limpiar Tara
                    </button>
                </div>

            </div>

        </div>

    </div>

    <!-- Pie de Pagina -->
    <div class="footer">
        FIRMWARE NODO HELTEC V3 | CONTROL LORAWAN CHIRPSTACK v4 | PROTOCOLO SDN 204B
    </div>

    <script>
        let isTransmitting = false;
        let txTimeoutTimer = null;
        let txCountdownInterval = null;
        let txSecondsElapsed = 0;

        async function updateStatus() {
            try {
                const response = await fetch('/api/status');
                if (!response.ok) return;
                const data = await response.json();

                // 1. Datos Principales de Peso
                const netKg = data.weight_kg;
                const grossKg = data.gross_kg;
                const lbs = (netKg * 2.20462).toFixed(2);
                const pct = data.hopper_pct;

                document.getElementById('weight-kg').textContent = netKg.toFixed(2);
                document.getElementById('weight-lbs').textContent = lbs + ' lbs';
                document.getElementById('gross-kg').textContent = grossKg.toFixed(2) + ' kg';
                document.getElementById('hopper-pct').textContent = pct.toFixed(1) + '%';
                document.getElementById('hopper-progress').style.width = Math.min(100, Math.max(0, pct)) + '%';

                // 2. Dinamica de Esquematico SVG
                const maxFeedHeight = 135;
                const fillHeight = (pct / 100.0) * maxFeedHeight;
                const feedRect = document.getElementById('svg-feed-rect');
                const feedTop = document.getElementById('svg-feed-top');

                feedRect.setAttribute('y', 270 - fillHeight);
                feedRect.setAttribute('height', fillHeight);
                feedTop.setAttribute('cy', 270 - fillHeight);
                feedTop.setAttribute('rx', 35 + (fillHeight * 0.45));

                document.getElementById('svg-volt-pill').textContent = data.voltage_v.toFixed(3) + ' V | GPIO 1';

                const loadBadge = document.getElementById('load-status-badge');
                if (pct <= 5.0) {
                    loadBadge.textContent = 'TOLVA CASI VACIA';
                    loadBadge.style.background = '#450a0a';
                    loadBadge.style.color = '#fecaca';
                } else if (pct >= 90.0) {
                    loadBadge.textContent = 'TOLVA LLENA';
                    loadBadge.style.background = '#451a03';
                    loadBadge.style.color = '#fde68a';
                } else {
                    loadBadge.textContent = 'ESTABLE';
                    loadBadge.style.background = '#064e3b';
                    loadBadge.style.color = '#6ee7b7';
                }

                // 3. Monitor de Carga en Vivo (Calibracion)
                document.getElementById('cal-live-v').textContent = data.voltage_v.toFixed(3) + ' V';
                document.getElementById('cal-live-raw').textContent = data.raw_adc;
                document.getElementById('cal-live-kg').textContent = netKg.toFixed(2) + ' kg';

                const slopeInput = document.getElementById('manual-slope');
                if (document.activeElement !== slopeInput && !slopeInput.value) {
                    slopeInput.value = data.slope_v_kg.toFixed(5);
                }
                const zeroInput = document.getElementById('manual-zero');
                if (document.activeElement !== zeroInput && !zeroInput.value) {
                    zeroInput.value = data.zero_v.toFixed(3);
                }

                // 4. Metricas LoRaWAN y Time on Air
                document.getElementById('toa-ms').textContent = data.toa_ms.toFixed(1);
                document.getElementById('tx-duration-ms').textContent = data.tx_duration_ms;
                document.getElementById('dev-addr').textContent = data.dev_addr;
                document.getElementById('uplink-seq').textContent = data.uplink_seq;
                document.getElementById('dr-val').textContent = data.datarate;
                document.getElementById('sf-val').textContent = data.sf;
                document.getElementById('txp-val').textContent = data.tx_power;
                document.getElementById('rssi-val').textContent = data.rssi.toFixed(1);
                document.getElementById('snr-val').textContent = data.snr.toFixed(1);
                document.getElementById('sdn-cmd').textContent = data.last_sdn_cmd;

                const selDr = document.getElementById('sel-dr');
                if (document.activeElement !== selDr) {
                    selDr.value = data.datarate;
                }
                const selAdr = document.getElementById('sel-adr');
                if (document.activeElement !== selAdr) {
                    selAdr.value = data.adr ? '1' : '0';
                }
                const selConfirmed = document.getElementById('sel-confirmed');
                if (document.activeElement !== selConfirmed) {
                    selConfirmed.value = data.confirmed ? '1' : '0';
                }

                const loraStateBadge = document.getElementById('lora-state-badge');
                if (data.lora_joined) {
                    loraStateBadge.textContent = 'UNIDO (JOINED)';
                    loraStateBadge.style.background = '#064e3b';
                    loraStateBadge.style.color = '#6ee7b7';
                } else {
                    loraStateBadge.textContent = 'NO CONECTADO';
                    loraStateBadge.style.background = '#450a0a';
                    loraStateBadge.style.color = '#fecaca';
                }

                // Estado de Transmision (Sincronizacion de UI)
                const btnTx = document.getElementById('btn-tx');
                const btnCancel = document.getElementById('btn-cancel-tx');
                const banner = document.getElementById('tx-banner');
                const bannerText = document.getElementById('tx-banner-text');

                if (data.tx_in_progress) {
                    isTransmitting = true;
                    btnTx.disabled = true;
                    btnCancel.style.display = 'inline-flex';
                    banner.className = 'tx-status-banner transmitting';
                } else if (isTransmitting) {
                    // Acaba de finalizar el ciclo en el nodo
                    isTransmitting = false;
                    clearTimeout(txTimeoutTimer);
                    clearInterval(txCountdownInterval);

                    btnTx.disabled = false;
                    btnCancel.style.display = 'none';
                    document.getElementById('btn-tx-text').textContent = 'TRANSMITIR DATOS AHORA (UPLINK)';

                    if (data.last_tx_success) {
                        banner.className = 'tx-status-banner success';
                        bannerText.textContent = '✔ Transmision enviada a ChirpStack con exito. ToA: ' + data.toa_ms.toFixed(1) + ' ms | Total: ' + data.tx_duration_ms + ' ms';
                    } else {
                        banner.className = 'tx-status-banner error';
                        bannerText.textContent = 'Transmision finalizo con codigo RadioLib: ' + data.last_tx_code + ' (Reintente o revise el gateway)';
                    }
                }

            } catch (err) {
                console.warn('Error en sondeo de telemetria:', err);
            }
        }

        setInterval(updateStatus, 1000);
        updateStatus();

        // Disparo Manual LoRaWAN con Contador y Timeout de Seguridad
        async function triggerLoraTx() {
            const btn = document.getElementById('btn-tx');
            const btnCancel = document.getElementById('btn-cancel-tx');
            const banner = document.getElementById('tx-banner');
            const bannerText = document.getElementById('tx-banner-text');

            btn.disabled = true;
            btnCancel.style.display = 'inline-flex';
            document.getElementById('btn-tx-text').textContent = 'TRANSMITIENDO UPLINK... (0s)';

            banner.className = 'tx-status-banner transmitting';
            bannerText.textContent = 'Enviando paquete LoRa a ChirpStack...';

            isTransmitting = true;
            txSecondsElapsed = 0;
            clearInterval(txCountdownInterval);
            txCountdownInterval = setInterval(() => {
                txSecondsElapsed++;
                document.getElementById('btn-tx-text').textContent = 'TRANSMITIENDO... (' + txSecondsElapsed + 's)';
            }, 1000);

            // Timeout de proteccion ampliado a 20s
            clearTimeout(txTimeoutTimer);
            txTimeoutTimer = setTimeout(() => {
                cancelLoraTx(true);
            }, 20000);

            try {
                const res = await fetch('/api/send_lora', { method: 'POST' });
                const json = await res.json();
                console.log('TX Solicitado:', json);
            } catch (e) {
                cancelLoraTx(false, 'Fallo al comunicar con nodo: ' + e);
            }
        }

        // Cancelacion de Transmision y Restablecimiento de Boton
        async function cancelLoraTx(isTimeout = false, customMsg = null) {
            clearTimeout(txTimeoutTimer);
            clearInterval(txCountdownInterval);

            const btn = document.getElementById('btn-tx');
            const btnCancel = document.getElementById('btn-cancel-tx');
            const banner = document.getElementById('tx-banner');
            const bannerText = document.getElementById('tx-banner-text');

            try {
                await fetch('/api/lora/cancel', { method: 'POST' });
            } catch (e) {}

            btn.disabled = false;
            btnCancel.style.display = 'none';
            document.getElementById('btn-tx-text').textContent = 'TRANSMITIR DATOS AHORA (UPLINK)';
            isTransmitting = false;

            banner.className = 'tx-status-banner error';
            if (customMsg) {
                bannerText.textContent = customMsg;
            } else if (isTimeout) {
                bannerText.textContent = 'Tiempo de espera superado. Boton restablecido.';
            } else {
                bannerText.textContent = 'Transmision cancelada por el usuario. Boton restablecido.';
            }
        }

        async function rejoinOtaa() {
            if (!confirm('Desea forzar una nueva negociacion de unirse (OTAA Rejoin) con ChirpStack?')) return;
            try {
                const res = await fetch('/api/lora/rejoin', { method: 'POST' });
                const json = await res.json();
                alert('Secuencia de Join iniciada en el nodo.');
            } catch (e) {
                alert('Error al solicitar Rejoin: ' + e);
            }
        }

        async function updateLoraConfig() {
            const dr = document.getElementById('sel-dr').value;
            const adr = document.getElementById('sel-adr').value;
            const confirmed = document.getElementById('sel-confirmed').value;

            try {
                await fetch('/api/lora/config?dr=' + dr + '&adr=' + adr + '&confirmed=' + confirmed, { method: 'POST' });
            } catch (e) {
                console.error('Error actualizando configuracion LoRa:', e);
            }
        }

        // Calibracion
        async function setZero() {
            if (!confirm('Confirma que la tolva se encuentra completamente vacia para fijar el Cero?')) return;
            try {
                const res = await fetch('/api/cal_zero', { method: 'POST' });
                const d = await res.json();
                alert('Cero calibrado en: ' + d.zero_v.toFixed(4) + ' V');
                document.getElementById('manual-zero').value = d.zero_v.toFixed(4);
            } catch (e) { alert('Error: ' + e); }
        }

        async function setSpan() {
            const spanKg = parseFloat(document.getElementById('span-weight').value);
            if (!spanKg || spanKg <= 0) {
                alert('Ingrese un valor de masa patron valido mayor a 0 kg');
                return;
            }
            if (!confirm('Confirma que coloco ' + spanKg + ' kg sobre la balanza?')) return;

            try {
                const res = await fetch('/api/cal_span?weight=' + spanKg, { method: 'POST' });
                const d = await res.json();
                if (d.success) {
                    alert('Span calibrado correctamente. Nueva pendiente: ' + d.slope_v_kg.toFixed(5) + ' V/kg');
                    document.getElementById('manual-slope').value = d.slope_v_kg.toFixed(5);
                } else {
                    alert('Error al calibrar span: ' + d.message);
                }
            } catch (e) { alert('Error: ' + e); }
        }

        async function setManualSlope() {
            const slope = parseFloat(document.getElementById('manual-slope').value);
            if (!slope) return;
            try {
                const res = await fetch('/api/cal_manual?slope=' + slope, { method: 'POST' });
                const d = await res.json();
                alert('Pendiente actualizada a ' + d.slope_v_kg.toFixed(5) + ' V/kg');
            } catch (e) { alert('Error: ' + e); }
        }

        async function setManualZero() {
            const zero = parseFloat(document.getElementById('manual-zero').value);
            if (isNaN(zero)) return;
            try {
                const res = await fetch('/api/cal_manual?zero=' + zero, { method: 'POST' });
                const d = await res.json();
                alert('Voltaje de cero actualizado a ' + d.zero_v.toFixed(4) + ' V');
            } catch (e) { alert('Error: ' + e); }
        }

        async function setTare() {
            try {
                await fetch('/api/tare', { method: 'POST' });
                alert('Tara aplicada a la lectura actual.');
            } catch (e) { alert('Error: ' + e); }
        }

        async function clearTare() {
            try {
                await fetch('/api/tare_clear', { method: 'POST' });
                alert('Tara eliminada.');
            } catch (e) { alert('Error: ' + e); }
        }

        async function saveToNvs() {
            try {
                await fetch('/api/cal_save', { method: 'POST' });
                alert('Calibracion almacenada en memoria no volatil (NVS Flash).');
            } catch (e) { alert('Error: ' + e); }
        }

        async function resetCalibration() {
            if (!confirm('Restablecer todos los parametros de calibracion a los valores de fabrica?')) return;
            try {
                await fetch('/api/cal_reset', { method: 'POST' });
                alert('Valores de calibracion restablecidos.');
                location.reload();
            } catch (e) { alert('Error: ' + e); }
        }
    </script>
</body>
</html>
)rawliteral";

#endif // WEB_INTERFACE_H
