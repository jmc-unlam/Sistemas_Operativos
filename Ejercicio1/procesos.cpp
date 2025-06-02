#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <unistd.h>
#include <cstdlib>
#include <signal.h>
#include <sys/wait.h>
#include <limits.h>
#include <cmath>

#define VECTOR_SIZE 10       
#define MEMORIA_COMPARTIDA "/mi_vector"  
#define NUM_HIJOS 4          

// Estructura que contendrá el vector y los semáforos
struct T_Dato_Compartido {
    int vector[VECTOR_SIZE];
    sem_t semaforo_principal;                  // Semáforo principal (acceso al vector)
    sem_t semaforo_hijo[NUM_HIJOS];     // Un semáforo por hijo para sincronización
    bool seguir;
    int iteraciones;
};


// Manejador de señales para detener el programa
T_Dato_Compartido* datos_global = nullptr;
void atender_senial(int signal) {
    if (datos_global != nullptr) {
        datos_global->seguir = false;
    }
}

// manejo desbordamiento
int suma_segura(int a, int b) {
    if (b > 0 && a > INT_MAX - b) return INT_MAX;
    if (b < 0 && a < INT_MIN - b) return INT_MIN;
    return a + b;
}

int resta_segura(int a, int b) {
    if (b > 0 && a < INT_MIN + b) return INT_MIN;
    if (b < 0 && a > INT_MAX + b) return INT_MAX;
    return a - b;
}

int mult_segura(int a, double b) {
    double resultado = static_cast<double>(a) * b;
    if (resultado > INT_MAX) return INT_MAX;
    if (resultado < INT_MIN) return INT_MIN;
    return static_cast<int>(round(resultado));
}

int div_segura(int a, int b) {
    if (b == 0) return a;  
    if (a == INT_MIN && b == -1) return INT_MAX;
    return a / b;
}

int main() {
    signal(SIGINT, atender_senial);

    //Crear o abrir memoria compartida con nombre
    int shm_fd = shm_open(MEMORIA_COMPARTIDA, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Error al crear memoria compartida");
        exit(1);
    }
    if (ftruncate(shm_fd, sizeof(T_Dato_Compartido)) == -1) {
        perror("Error al configurar tamaño de memoria compartida");
        exit(1);
    }

    // Mapear la memoria compartida
    T_Dato_Compartido* datos = (T_Dato_Compartido*)mmap(NULL, sizeof(T_Dato_Compartido), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (datos == MAP_FAILED) {
        perror("Error al mapear memoria compartida");
        exit(1);
    }    
    datos_global = datos;

    // Inicializar el vector compartido con múltiplos de 10
    for (int i = 0; i < VECTOR_SIZE; ++i) {
        datos->vector[i] = (i + 1) * 10;
    }
    datos->seguir = true;
    datos->iteraciones = 0;

    // Inicializar semáforos anónimos en la memoria compartida
    if (sem_init(&datos->semaforo_principal, 1, 1) == -1) {  // Semáforo principal, compartido entre procesos
        perror("Error al inicializar semáforo principal");
        exit(1);
    }

    for (int i = 0; i < NUM_HIJOS; ++i) {
        int valor_inicial = 0;
        if (i == 0) {
            valor_inicial = 1;
        }
        if (sem_init(&datos->semaforo_hijo[i], 1, valor_inicial) == -1) {  // Cada hijo empieza bloqueado
            perror("Error al inicializar semáforo de hijo");
            exit(1);
        }
    }

    // Crear procesos hijos
    pid_t pids[NUM_HIJOS];

    for (int i = 0; i < NUM_HIJOS; ++i) {
        pids[i] = fork();

        if (pids[i] == -1) {
            perror("Error al crear proceso hijo");
            exit(1);
        } else if (pids[i] == 0) {
            // Código del hijo

            while (datos->seguir) {
                // Esperar su turno
                sem_wait(&datos->semaforo_hijo[i]);

                if (!datos->seguir) break;

                // Acceder al vector compartido
                sem_wait(&datos->semaforo_principal);

                // Realizar operación según el hijo
                switch (i) {
                    case 0: // Hijo 1: Suma el valor siguiente en posiciones impares
                        for (int j = 1; j < VECTOR_SIZE; j += 2) {
                            //siguiente depende si es el ultimo es la posicion 0 
                            int siguiente = (j == VECTOR_SIZE - 1) ? 0 : j + 1;
                            datos->vector[j] = suma_segura(datos->vector[j], datos->vector[siguiente]);
                            
                        }
                        break;
                    case 1: // Hijo 2: Resta el valor anterior en posiciones pares
                        for (int j = 0; j < VECTOR_SIZE; j += 2) {
                            //anterior depende si es primero es la ultima posicion
                            int anterior = (j == 0) ? VECTOR_SIZE - 1 : j - 1;
                            datos->vector[j] = resta_segura(datos->vector[j], datos->vector[anterior]);
                        }
                        break;
                    case 2: // Hijo 3: Divide por el valor anterior en posiciones pares
                        for (int j = 0; j < VECTOR_SIZE; j += 2) {
                            //anterior depende si es primero es la ultima posicion
                            int anterior = (j == 0) ? VECTOR_SIZE - 1 : j - 1;
                            if (datos->vector[anterior] != 0) {  // Solo dividir si no es cero
                                datos->vector[j] = div_segura(datos->vector[j], datos->vector[anterior]);
                            }
                        }
                        break;
                    case 3: // Hijo 4: Multiplica por un 10% del valor en posiciones pares
                        for (int j = 0; j < VECTOR_SIZE; j += 2) {
                            datos->vector[j] = mult_segura(datos->vector[j], 1.10);
                        }
                        break;
                }
                datos->iteraciones++;

                // Liberar acceso al vector
                sem_post(&datos->semaforo_principal);

                // Si no es el último hijo, liberar al siguiente
                int siguiente_hijo = i + 1;
                if (siguiente_hijo >= NUM_HIJOS) {
                    siguiente_hijo = 0;
                }
                sem_post(&datos->semaforo_hijo[siguiente_hijo]);
                usleep(100000);
            }

            // Salir del hijo
            exit(0);
        }
    }

    // Proceso padre: iniciar el ciclo notificando al primer hijo
    sem_post(&datos->semaforo_hijo[0]);

    // Padre espera entrada del usuario
    std::cout << "Presione Enter para terminar...\n";
    std::cin.ignore();

    // Ordenar terminación
    datos->seguir = false;

    // Notificar a todos los hijos para que terminen
    for (int i = 0; i < NUM_HIJOS; ++i) {
        sem_post(&datos->semaforo_hijo[i]);
    }

    // Esperar a que los hijos terminen
    for (int i = 0; i < NUM_HIJOS; ++i) {
        waitpid(pids[i], NULL, 0);
    }

    // Mostrar estado final del vector
    std::cout << "\nIteraciones completadas: " << datos->iteraciones << "\nVector final:\n";
    for (int i = 0; i < VECTOR_SIZE; ++i) {
        std::cout << datos->vector[i] << " ";
    }
    std::cout << "\n";

    // Limpiar recursos

    // Destruir semáforos
    sem_destroy(&datos->semaforo_principal);
    for (int i = 0; i < NUM_HIJOS; ++i) {
        sem_destroy(&datos->semaforo_hijo[i]);
    }

    // Desmapear y cerrar memoria compartida
    munmap(datos, sizeof(T_Dato_Compartido));
    close(shm_fd);

    // Eliminar objeto de memoria compartida
    shm_unlink(MEMORIA_COMPARTIDA);

    std::cout << "Programa finalizado y recursos limpiados.\n" << std::endl;

    return 0;
}
