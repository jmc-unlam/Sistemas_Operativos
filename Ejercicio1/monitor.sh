#!/bin/bash

EXECUTABLE="procesos.bin"
SOURCE="procesos.cpp"
TIMEOUT=30  # Tiempo máximo de ejecución si no hay interacción
UPDATE_INTERVAL=1  # Segundos entre actualizaciones
VECTOR_SIZE=10

mostrar_vector() {
    echo -n "Vector: "
    if [ -e "/dev/shm/mi_vector" ]; then
        dd if=/dev/shm/mi_vector bs=4 count=$VECTOR_SIZE 2>/dev/null | od -i -An -v | tr '\n' ' '
        echo
    else
        echo "no disponible"
    fi
}

monitor() {
    while true; do
        clear
        echo "=== Monitor de Procesos ==="
        echo "Tiempo restante: $((TIMEOUT - COUNT * UPDATE_INTERVAL)) segundos (Enter para terminar)"

        mostrar_vector

        echo -e "\n- Procesos padre:"
        ps -p $PROGRAM_PID

        echo -e "\n- Procesos hijos:"
        ps --ppid $PROGRAM_PID
        
        # Mostrar top filtrado
        echo -e "\n[Uso de recursos - refresca cada ${UPDATE_INTERVAL}s]"
        top -b -n 1 -p "$(pgrep -P "$PROGRAM_PID" | tr '\n' ',')$PROGRAM_PID" | head -12
        
        # Mostrar memoria compartida
        echo -e "\n[Memoria compartida]"
        ls -l /dev/shm/ | grep -i mi_vector 2>/dev/null || echo "No visible"
        
        # Comprobar si el programa sigue ejecutándose
        if ! ps -p $PROGRAM_PID > /dev/null; then
            echo -e "\nEl programa ha terminado por sí mismo."
            break
        fi
        
        # Esperar entrada o timeout
        read -t 0.1 -n 1 input
        if [[ $input ]]; then
            break
        fi
        sleep $UPDATE_INTERVAL
        
        ((COUNT++))
        if [ $((COUNT * UPDATE_INTERVAL)) -ge $TIMEOUT ]; then
            echo -e "\nTimeout alcanzado. Terminando programa..."
            break
        fi
    done
}

# Compilación
echo "Compilando el programa..."
g++ -o $EXECUTABLE $SOURCE -lrt -lpthread
if [ $? -ne 0 ]; then
    echo "Error: Falló la compilación"
    exit 1
fi
echo "Compilación exitosa"

# Mostrar estado inicial de memoria compartida
echo -e "\n=== Estado Inicial ==="
echo "- Memoria compartida (/dev/shm):"
ls -l /dev/shm/

# Crear pipe y ejecutar el programa
echo -e "\n=== Ejecutando el programa ==="
mkfifo mypipe
exec 3<>mypipe 
./"$EXECUTABLE" < mypipe & 
PROGRAM_PID=$! 

echo "PID del proceso principal: $PROGRAM_PID"

# Esperar inicialización
sleep 3
# Iniciar monitoreo
COUNT=0

monitor

echo "Terminando programa..."
echo "" > mypipe

wait $PROGRAM_PID 2>/dev/null

# Limpiar tubería
exec 3>&-
rm mypipe

echo -e "\n=== Limpieza completada ==="

echo -e "\n=== Estado FINAL de memoria compartida ==="
ls -l /dev/shm/ 
