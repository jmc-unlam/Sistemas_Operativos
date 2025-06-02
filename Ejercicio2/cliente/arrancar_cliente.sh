#!/bin/bash

EXECUTABLE="mi_cliente.bin"
SOURCE="cliente_archivos.cpp"

# Compilación
echo "Compilando el programa..."
g++ -o $EXECUTABLE $SOURCE -lpthread -pthread
if [ $? -ne 0 ]; then
    echo "Error: Falló la compilación"
else 
    echo "Compilación exitosa"
fi

./"$EXECUTABLE"
