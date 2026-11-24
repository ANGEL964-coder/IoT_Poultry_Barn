#!/usr/bin/env bash
# ==============================================================================
# Detener Dashboard SDN LoRaWAN
# ==============================================================================

PID=$(lsof -ti :8085,8084 2>/dev/null | tr '\n' ' ')
if [ -z "$PID" ]; then
    PID=$(pgrep -f "dashboard-sdn" | tr '\n' ' ')
fi
if [ -n "$PID" ]; then
    echo "Deteniendo proceso Dashboard Java (PID: $PID)..."
    kill $PID 2>/dev/null
    sleep 2
    kill -9 $PID 2>/dev/null
    echo "✔ Dashboard detenido exitosamente."
else
    echo "ℹ️ No hay ningún proceso escuchando en el puerto 8085/8084."
fi
