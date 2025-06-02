#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <vector>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <atomic>
#include <csignal>

// Configuración
int sockfd;
const std::string SERVER_IP = "127.0.0.1";
const int PORT = 8080;
const size_t MAX_FILE_SIZE = 2 * 1024 * 1024; // 2MB

// Muestra el menú de comandos
void mostrar_menu() {
    std::cout << "\n--- Menú de Comandos ---\n";
    std::cout << "LIST             - Listar archivos disponibles\n";
    std::cout << "UPLOAD <archivo> - Subir un archivo al servidor\n";
    std::cout << "DOWNLOAD <archivo> - Descargar un archivo del servidor\n";
    std::cout << "EXIT             - Salir del cliente\n";
    std::cout << "Ingrese su comando: ";
}

// Lee línea completa del teclado
std::string leer_comando() {
    std::string cmd;
    std::getline(std::cin, cmd);
    return cmd;
}

// Valida si el comando es válido
bool validar_comando(const std::string& cmd) {
    if (cmd.empty()) return false;

    if (cmd == "LIST") return true;
    else if (cmd.substr(0, 6) == "UPLOAD" && cmd.size() > 7 && cmd[6] == ' ') return true;
    else if (cmd.substr(0, 8) == "DOWNLOAD" && cmd.size() > 9 && cmd[8] == ' ') return true;
    else if (cmd == "EXIT") return true;

    std::cerr << "Comando no reconocido.\n";
    return false;
}

// Obtiene el nombre del archivo de un comando DOWNLOAD o UPLOAD
std::string obtener_nombre_archivo(const std::string& cmd) {
    size_t pos = cmd.find(' ');
    if (pos == std::string::npos) return "";
    return cmd.substr(pos + 1);
}

// Manejador de señales
std::atomic<bool> keep_running(true);
void atender_senial(int signal) {
    keep_running = false;
    close(sockfd);
}


int main() {
    std::signal(SIGINT, atender_senial);
    std::signal(SIGTERM, atender_senial);
    std::signal(SIGHUP, atender_senial);

    struct sockaddr_in serv_addr;

    // Crear socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Error al crear socket\n";
        exit(EXIT_FAILURE);
    }

    // Configurar dirección del servidor
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP.c_str(), &serv_addr.sin_addr) <= 0) {
        std::cerr << "Dirección inválida\n";
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Conectar al servidor
    std::cout << "Conectado al servidor en " << SERVER_IP << ":" << PORT << "..............\n";
    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "No se pudo conectar al servidor\n";
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    //std::cout << "Conectado al servidor en " << SERVER_IP << ":" << PORT << "\n";
    char response[50];
    ssize_t bytes_read = read(sockfd, response, sizeof(response) - 1);
    if (bytes_read <= 0) {
        std::cerr << "Conexión cerrada por el servidor\n";
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    response[bytes_read] = '\0';
    std::cout << "Servidor dice: " << response << "\n";
    if (strncmp(response, "ERROR", 5) == 0) {
        close(sockfd);
        exit(0);
    }


    std::string comando;
    while (keep_running) {


        mostrar_menu();
        comando = leer_comando();

        if (comando.empty()) continue;

        if (!validar_comando(comando)) continue;

        if (comando.substr(0, 4) == "EXIT") {
            write(sockfd, comando.c_str(), comando.size());
            std::cout << "Saliendo...\n";
            break;
        }

        // Enviar comando al servidor
        write(sockfd, comando.c_str(), comando.size());

        char response[1024];
        ssize_t bytes_read = read(sockfd, response, sizeof(response) - 1);
        if (bytes_read <= 0) {
            std::cerr << "Conexión cerrada por el servidor\n";
            break;
        }
        response[bytes_read] = '\0';
        std::cout << "Servidor dice: " << response << "\n";

        if (strncmp(response, "ERROR", 5) == 0) {
            std::cout << "Operación rechazada por el servidor.\n";
            continue;
        }

        if (strncmp(response, "OK", 2) == 0) {
            std::cout << "Servidor acepto la Operación.\n";
        }

        //procesamos el comando del lado del cliente
        if (strncmp(comando.c_str(), "UPLOAD", 6) == 0) {
            std::string filename = obtener_nombre_archivo(comando);
            if (filename.empty()) {
                std::cerr << "Nombre de archivo inválido.\n";
                continue;
            }

            std::ifstream file(filename, std::ios::binary | std::ios::ate);
            if (!file) {
                std::cerr << "Archivo local no encontrado: " << filename << "\n";
                continue;
            }

            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);

            std::vector<char> buffer(size);
            if (!file.read(buffer.data(), size)) {
                std::cerr << "Error al leer archivo local\n";
                continue;
            }

            // Enviar tamaño en formato de red
            uint32_t net_size = htonl(size);
            write(sockfd, &net_size, sizeof(net_size));
            // Enviar archivo completo
            write(sockfd, buffer.data(), size);


            // Leer confirmación final del servidor
            bytes_read = read(sockfd, response, sizeof(response) - 1);
            if (bytes_read <= 0) {
                std::cerr << "Conexión perdida tras subir archivo\n";
                break;
            }
            response[bytes_read] = '\0';
            std::cout << "Confirmación del servidor: " << response << "\n";

        } else if (strncmp(comando.c_str(), "DOWNLOAD", 8) == 0) {
            std::string filename = obtener_nombre_archivo(comando);
            if (filename.empty()) {
                std::cerr << "Nombre de archivo inválido.\n";
                continue;
            }

            // Leer respuesta inicial del servidor
            char response[32];
            bytes_read = read(sockfd, response, sizeof(response) - 1);
            if (bytes_read <= 0) {
                std::cerr << "Conexión cerrada por el servidor\n";
                continue;
            }
            response[bytes_read] = '\0';
            std::cout << "Servidor dice: " << response;

            if (strncmp(response, "ERROR", 5) == 0) {
                std::cout << "Operación rechazada por el servidor.\n";
                continue;
            } else if (strncmp(response, "OK", 2) == 0) {
                // Enviar confirmación al servidor (1 byte)
                write(sockfd, "A", 1);

                // Recibir el archivo completo
                char file_buffer[MAX_FILE_SIZE];
                ssize_t total_bytes = read(sockfd, file_buffer, MAX_FILE_SIZE);

                if (total_bytes <= 0) {
                    std::cerr << "Error al recibir archivo.\n";
                    continue;
                }

                // Guardar archivo localmente
                std::ofstream out(filename, std::ios::binary);
                if (!out) {
                    std::cerr << "No se puede crear el archivo local.\n";
                    continue;
                }

                out.write(file_buffer, total_bytes);
                out.close();

                std::cout << "Archivo '" << filename << "' descargado correctamente (" 
                          << total_bytes << " bytes).\n";
            }
            continue;
        } else if (strncmp(comando.c_str(), "LIST", 4) == 0) {
            // Buffer para recibir la respuesta completa
            char buffer[4096]; // Tamaño suficiente para listados normales
            
            // Recibir la respuesta del servidor
            ssize_t bytes_recibidos = read(sockfd, buffer, sizeof(buffer) - 1);
            
            if (bytes_recibidos > 0) {
                // Asegurar terminación nula
                buffer[bytes_recibidos] = '\0';
                
                // Mostrar el listado directamente
                std::cout << "\nArchivos disponibles:\n";
                std::cout << "-------------------\n";
                std::cout << buffer;
                std::cout << "-------------------\n";
            } else if (bytes_recibidos == 0) {
                std::cerr << "Error: Conexión cerrada por el servidor\n";
            } else {
                std::cerr << "Error al recibir listado: " << strerror(errno) << "\n";
            }
            continue;
        } else {
            std::cout << "Algo salio mal.\n";
            continue;
        }
    }

    close(sockfd);
    return 0;
}
