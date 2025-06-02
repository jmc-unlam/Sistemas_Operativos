# Sistemas_Operativos
Ejercicio 1: Sistema de procesamiento de datos en paralelo; esto lo haremos mediante un 
vector compartido de 10 enteros inicializados en valores de múltiplos de 10, en el cual 
aplicaremos distintas acciones por medio de procesos hijos. 
Cada proceso hijo intentará modificar el vector compartido concurrentemente.

La memoria compartida la utilizaremos mediante un vector que en este caso será de números enteros.  
También emplearemos semáforos para sincronizar el acceso a la memoria compartida. 
Un semáforo principal y 4 semáforos por hijo para que un hijo espere a que otro termine. 
Una vez que todos los procesos hijos terminen de ejecutar sus respectivas acciones, 
el proceso padre se encargará de mostrar los resultados finales; 
y por último se liberarán los recursos utilizados durante la ejecución del programa.

Ejercicio 2: El servidor de archivos maneja múltiples clientes de forma concurrente. 
Los clientes pueden subir, descargar y listar archivos almacenados en el servidor. 

Servidor TCP concurrente:
Subir (UPLOAD) archivos al servidor.
Descargar (DOWNLOAD) archivos desde el servidor.
Listar (LIST) para ver los archivos del servidor.
Limitar simultáneamente hasta 3 conexiones activas usando un semaforo sin nombre .
Establecer un tamaño máximo por archivo: 2MB .
Mantener una carpeta local como repositorio de archivos compartidos.
Garantizar la finalización limpia del servidor cuando no haya clientes conectados.

Cliente:
Se conecta al servidor.
Ofrece un menú para las operaciones:
Enviar un archivo (UPLOAD).
Solicitar descarga (DOWNLOAD).
Listar archivos (LIST).

Ejemplo:
Cliente escribe "UPLOAD archivo.txt" y el Servidor responde: "OK"
El Servidor lo guarda en una carpeta local y confirma el fin de transferencia.
Cliente escribe: "DOWNLOAD archivo.txt"Si existe responde OK 
y el cliente recibe el archivo desde donde se encuentre la terminal.
En caso de no estar escribe "Archivo No Encontrado

