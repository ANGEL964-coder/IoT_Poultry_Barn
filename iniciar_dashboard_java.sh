#!/usr/bin/env bash
# ==============================================================================
# Iniciar Dashboard SDN LoRaWAN (Spring Boot 3 + Paho MQTT + WebSockets)
# ==============================================================================

PROJECT_DIR="/home/server/CONTROLADOR_SDN/dashboard-java"
JAR_FILE="${PROJECT_DIR}/target/dashboard-sdn-1.0.0.jar"
if [ -d "/home/server/tools/jdk-21.0.13+9" ]; then
    JAVA_HOME="/home/server/tools/jdk-21.0.13+9"
elif [ -d "/usr/lib/jvm/java-21-openjdk-arm64" ]; then
    JAVA_HOME="/usr/lib/jvm/java-21-openjdk-arm64"
elif [ -d "/usr/lib/jvm/default-java" ]; then
    JAVA_HOME="/usr/lib/jvm/default-java"
else
    JAVA_HOME=$(dirname $(dirname $(readlink -f $(which java))))
fi
LOG_FILE="/home/server/CONTROLADOR_SDN/dashboard_java.log"

if [ ! -f "$JAR_FILE" ]; then
    echo "❌ Error: No se encontró el archivo JAR en $JAR_FILE"
    echo "Compilando proyecto..."
    cd "$PROJECT_DIR" || exit 1
    export JAVA_HOME="$JAVA_HOME"
    export PATH="$JAVA_HOME/bin:/home/server/tools/apache-maven-3.9.9/bin:$PATH"
    export LD_LIBRARY_PATH="$JAVA_HOME/lib:$LD_LIBRARY_PATH"
    mvn clean package -DskipTests
fi

# Verificar si ya está en ejecución en el puerto 8085
EXISTING_PID=$(lsof -ti :8085 2>/dev/null)
if [ -n "$EXISTING_PID" ]; then
    echo "⚠️ El Dashboard Java ya se encuentra activo (PID: $EXISTING_PID) en el puerto 8085."
    exit 0
fi

echo "🚀 Iniciando Dashboard Java en http://0.0.0.0:8085 ..."
export JAVA_HOME="$JAVA_HOME"
export LD_LIBRARY_PATH="$JAVA_HOME/lib:$LD_LIBRARY_PATH"

setsid -f "$JAVA_HOME/bin/java" -jar "$JAR_FILE" </dev/null > "$LOG_FILE" 2>&1
sleep 5
DASH_PID=$(pgrep -f "dashboard-sdn" | head -n 1)

if ps -p $DASH_PID > /dev/null; then
    CURRENT_IP=$(hostname -I 2>/dev/null | awk '{print $1}')
    [ -z "$CURRENT_IP" ] && CURRENT_IP="10.153.116.23"
    TAILSCALE_IP=$(tailscale ip -4 2>/dev/null)
    [ -z "$TAILSCALE_IP" ] && TAILSCALE_IP="100.69.222.104"
    echo "✔ Dashboard iniciado exitosamente con PID $DASH_PID."
    echo "🔒 Acceso Seguro (HTTPS):"
    echo "🌐 Acceso local:     https://localhost:8085"
    echo "🌐 Acceso en LAN:    https://${CURRENT_IP}:8085"
    echo "🌐 Acceso Tailscale: https://${TAILSCALE_IP}:8085"
    echo "📜 Logs en tiempo real: tail -f $LOG_FILE"
else
    echo "❌ Error al iniciar el Dashboard. Revisa los logs en $LOG_FILE"
    cat "$LOG_FILE"
    exit 1
fi
