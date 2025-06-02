#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>
#include <csignal>
#include <atomic>
#include <list>
#include <mutex>

// Configuración
int server_fd;
const int PORT = 8080;
const int MAX_CLIENTS = 3;
const size_t MAX_FILE_SIZE = 2 * 1024 * 1024; // 2 MB
const char* ALMACEN_DIR = "./almacen/";

// Recursos compartidos
std::list<int> clientes;
std::mutex mutex_clientes;
sem_t semaforo_max_clientes;

// Función que maneja la comunicación con un cliente
void* atender_cliente(void* arg);

// Función que envía listado de archivos
void listar_contenido(int client_fd);

// Función que recibe y guarda un archivo
void recibir_archivo(int client_fd, const char* filename);

// Función que envía un archivo al cliente
void enviar_archivo(int client_fd, const char* filename);

// Manejador de señales
std::atomic<bool> keep_running(true);
void atender_senial(int signal);

int main() {
    std::signal(SIGINT, atender_senial);
    std::signal(SIGTERM, atender_senial);
    std::signal(SIGHUP, atender_senial);

    mkdir(ALMACEN_DIR, 0777);
    
    sem_init(&semaforo_max_clientes, 0, MAX_CLIENTS);

    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Crear socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Permitir reusar puerto
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Configurar dirección
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == -1) {
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Listen
    if (listen(server_fd, MAX_CLIENTS * 2) == -1) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    std::cout << "Servidor escuchando en puerto " << PORT << ". Máximo de conexiones: " << MAX_CLIENTS << std::endl;

    // Bucle principal: aceptar conexiones
    while (keep_running) {

        int* client_fd = new int;
        *client_fd = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (*client_fd == -1) {
            continue;
        }

        // Bloquea hasta que haya lugar
        if (sem_trywait(&semaforo_max_clientes) == 0) {
            write(*client_fd, "OK, Conexión Aceptada.\n", 24); // Envía READY al cliente
            std::cout << "Cliente Aceptado." << std::endl;

            // Agregar cliente a la lista
            mutex_clientes.lock();
            clientes.push_back(*client_fd);
            std::cout << "Clientes activos: " << clientes.size() << " / " << MAX_CLIENTS << std::endl;
            mutex_clientes.unlock();

            // Crear thread para atender al cliente
            pthread_t thread_id;
            if (pthread_create(&thread_id, NULL, atender_cliente, client_fd) != 0) {
                std::cout << "Error al crear thread" << std::endl;
                close(*client_fd);

                mutex_clientes.lock();
                clientes.remove(*client_fd);
                std::cout << "Clientes activos: " << clientes.size() << " / " << MAX_CLIENTS << std::endl;
                mutex_clientes.unlock();

                sem_post(&semaforo_max_clientes);
            } else {
                pthread_detach(thread_id);
            }
        }  else {
            write(*client_fd, "ERROR, Conexión rechazada, Servidor ocupado.", 44); // Informa al cliente que el servidor está ocupado
            std::cout << "Cliente rechazado, Servidor ocupado. Clientes activos: " << clientes.size() << " / " << MAX_CLIENTS << std::endl;
            close(*client_fd);
        }
        delete client_fd;
    }

    //Libero los recursos
    mutex_clientes.lock();
    for (int fd : clientes) {
      shutdown(fd, SHUT_RDWR);
      close(fd);
    }
    clientes.clear();
    mutex_clientes.unlock(); 

    close(server_fd);
    sem_destroy(&semaforo_max_clientes);
    return 0;
}

// Manejar conexión de cliente
void* atender_cliente(void* arg) {
    int client_fd = *(int*)arg;
    //delete (int*)arg;

    char buffer[1024];
    ssize_t bytes_read;

    std::cout << "Cliente conectado. Atendiendo..." << std::endl;

    while ((bytes_read = read(client_fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0'; // Terminador nulo

        std::string comando(buffer);

        if (comando.substr(0, 4) == "EXIT") {
            std::cout << "Cliente cerró conexión." << std::endl;
            break;
        }

        char command[32], filename[256];
        sscanf(buffer, "%s %s", command, filename);

        if (strncmp(command, "UPLOAD", 6) == 0) {
            write(client_fd, "OK\n", 4);
            recibir_archivo(client_fd, filename);
        } else if (strncmp(command, "DOWNLOAD", 8) == 0) {
            write(client_fd, "OK\n", 4);
            enviar_archivo(client_fd, filename);
        } else if (strncmp(command, "LIST", 4) == 0) {
            write(client_fd, "OK\n", 4);
            listar_contenido(client_fd);
        } else {
            write(client_fd, "ERROR Comando no reconocido\n", 27);
        }
    }

    // Eliminar de la lista
    mutex_clientes.lock();
    clientes.remove(client_fd);
    std::cout << "Clientes activos: " << clientes.size() << " / " << MAX_CLIENTS << std::endl;
    mutex_clientes.unlock();

    close(client_fd);
    sem_post(&semaforo_max_clientes);

    // Si no quedan clientes, apagar
    if (clientes.size() == 0) {
        std::cout << "Todos los clientes se han desconectado. Apagando servidor..." << std::endl;
        keep_running = false;
        shutdown(server_fd, SHUT_RDWR);
    }

    return nullptr;
}


// Recibir archivo del cliente
void recibir_archivo(int client_fd, const char* filename) {
    std::string filepath = std::string(ALMACEN_DIR) + "/" + filename;

    FILE* fp = fopen(filepath.c_str(), "wb");
    if (!fp) {
        write(client_fd, "ERROR No se pudo crear archivo\n", 31);
        return;
    }
    std::cout << "creo el archivo\n";

    // Leer tamaño del archivo
    uint32_t net_size;
    ssize_t bytes_read = read(client_fd, &net_size, sizeof(net_size));

    if (bytes_read < 0) {
        write(client_fd, "ERROR Al leer tamaño del archivo\n", 34);
        fclose(fp);
        remove(filepath.c_str());
        return;
    } else if (bytes_read == 0) {
        std::cerr << "Cliente cerró conexión antes de enviar tamaño\n";
        fclose(fp);
        remove(filepath.c_str());
        return;
    }
    size_t file_size = ntohl(net_size);

    if (file_size > MAX_FILE_SIZE) {
        std::cerr << "Tamaño excedido: " << file_size << " bytes.\n";
        write(client_fd, "ERROR Archivo supera 2MB\n", 27);
        fclose(fp);
        remove(filepath.c_str());
        return;
    }

    char* file_buffer = new char[file_size];
    ssize_t total = 0;
    while (total < file_size) {
        ssize_t bytes_read = read(client_fd, file_buffer + total, file_size - total);
        if (bytes_read <= 0) break;
        total += bytes_read;
    }
    if (total == file_size) {
        fwrite(file_buffer, 1, file_size, fp);
        write(client_fd, "OK Archivo recibido\n", 20);
    } else {
        write(client_fd, "ERROR Tamaño incompleto\n", 25);
    }

    fclose(fp);
}

// Enviar archivo al cliente
void enviar_archivo(int client_fd, const char* filename) {
    std::string filepath = std::string(ALMACEN_DIR) + "/" + filename;

    FILE* fp = fopen(filepath.c_str(), "rb");
    if (!fp) {
        write(client_fd, "ERROR Archivo no encontrado\n", 27);
        return;
    }
    std::cout << "primer write\n";

    // Primero enviamos solo el OK y esperamos confirmación
    usleep(500000);
    write(client_fd, "OK\n", 3);
    // Esperamos un byte de confirmación del cliente
    std::cout << "primer read\n";
    char ack;
    if (read(client_fd, &ack, 1) <= 0) {
        fclose(fp);
        return; // Cliente desapareció
    }

    // Ahora sí enviamos el archivo completo
    fseek(fp, 0, SEEK_END);
    size_t file_size = ftell(fp);
    rewind(fp);

    char* buffer = new char[file_size];
    fread(buffer, 1, file_size, fp);
    fclose(fp);
    std::cout << "ul5im write.\n";

    write(client_fd, buffer, file_size);
    delete[] buffer;

    std::cout << "Archivo '" << filename << "' enviado correctamente.\n";
}

// Enviar listado de archivos en ./almacen/
void listar_contenido(int client_fd) {
    DIR* dir;
    struct dirent* ent;
    std::string response;

    if ((dir = opendir(ALMACEN_DIR)) != NULL) {
        bool files_found = false;
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_type == DT_REG) { // Solo archivos regulares
                response += std::string(ent->d_name) + "\n";
                files_found = true;
            }
        }
        closedir(dir);
        if (!files_found) {
            response += "(Carpeta vacía o sin archivos visibles)\n"; 
        }
        usleep(100000);
        write(client_fd, response.c_str(), response.size());
    } else {
        write(client_fd, "ERROR No se pudo leer carpeta: Permiso denegado o no existe\n", 60);
    }
}

void atender_senial(int signal) {
    keep_running = false;
    shutdown(server_fd, SHUT_RDWR);
}
