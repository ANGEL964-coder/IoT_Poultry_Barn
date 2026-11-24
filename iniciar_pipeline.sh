#!/usr/bin/env bash
# Script para iniciar el pipeline de borde SDN en la Raspberry Pi
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "========================================================"
echo "🚀 INICIANDO PIPELINE SDN-LoRa (FASE 1)"
echo "========================================================"
CURRENT_IP=$(hostname -I | awk '{print $1}')
echo "IP Local del Servidor: ${CURRENT_IP:-10.245.31.23}"
echo "Puerto MQTT:           1883"
echo "Puerto Redis:          6379"
echo "========================================================"

# Verificar entorno virtual
if [ ! -d ".venv" ]; then
    echo "[!] Creando entorno virtual..."
    python3 -m venv .venv
    source .venv/bin/activate
    pip install -r requirements.txt
else
    source .venv/bin/activate
fi

# Iniciar script de recepción
python3 src/edge/test_pipeline.py
