#!/bin/bash

EXECUTABLE="mi_servidor.bin"
SOURCE="servidor_archivos.cpp"

# Compilación
echo "Compilando el programa..."
g++ -o $EXECUTABLE $SOURCE -lpthread -pthread
if [ $? -ne 0 ]; then
    echo "Error: Falló la compilación"
    exit 1
fi
echo "Compilación exitosa"

./"$EXECUTABLE"
