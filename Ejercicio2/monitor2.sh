#!/bin/bash

# Configuración
PORT=8080
SERVER_NAME="mi_servidor"
INTERVAL=2  # Segundos entre refrescos

while true; do
    clear
    SERVER_PID=$(pgrep -f "$SERVER_NAME")
    if [ -n "$SERVER_PID" ]; then
        echo "=== MONITOREO BÁSICO DEL SERVIDOR ==="
        echo "Servidor PID: $SERVER_PID"
        echo "Puerto: $PORT"
        echo "Refrescando cada $INTERVAL segundos..."
        echo "Presiona Ctrl+C para salir"
        echo "===================================="

        # 1. THREADS (Concurrencia)
        echo "----- THREADS (hilos concurrentes) -----"
        ps -T -p $SERVER_PID
        
        # 2. CONEXIONES (Sockets)
        echo -e "\n----- CONEXIONES ACTIVAS (sockets) -----"
        netstat -tulnp | grep -E "(Proto|$PORT)"
        
        # 3. SOCKETS ABIERTOS
        echo -e "\n----- SOCKETS ABIERTOS -----"
        lsof -i :$PORT -a -p $SERVER_PID | sed 's/http-alt/8080/'
        
        # 4. USO DE RECURSOS
        echo -e "\n----- USO DE RECURSOS (Thread principal) -----"
        top -b -n 1 -H -p $SERVER_PID | head -n 12
    else
        echo "El servidor '$SERVER_NAME' no esta en ejecución."
    fi
        
    sleep $INTERVAL
done