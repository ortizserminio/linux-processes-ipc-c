// parking.c
//
// Este es el fichero del código C de la evaluación.
// Contiene multiples bloques indicados con la palabra TAREA que a su vez
// pueden contener uno o varios segmentos de código indicados con:
// ...
// Se trata de una o más lineas de código a rellenar.
// Se valorará la funcionalidad y limpieza del código. También se valoraran las
// comprobaciones necesarias para gestionar los posibles errores que puedan
// surgir durante la ejecución.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>
#include "sem.h"

#define BUF_SIZE 120

#define SHMSEED 77
#define SHMPERMISOS 0644

#define SSEED 200

#define MAX_VEHICULOS 100

#define NUM_ENTRADAS 2 // podría ser un argumento

#define ERROR_SALIDA 1

struct VEHICULO {
    char matricula[8];
    int llegada;
    int salida;
};

// Esta estructura almacena el estado global del parking. Se reservará en
// memoria compartida para que los distintos procesos tengan acceso a ella.
struct ESTADO {
    int capacidad;   // plazas totales
    int ocupacion;   // ocupación actual
    struct VEHICULO plazas[MAX_VEHICULOS];
};


int esNumero(char*);
int procesar_archivo(char*, struct VEHICULO**);
void imprimir_estado_aparcamiento(int, struct ESTADO*, int);
void proceso_entrada(int, int*, struct ESTADO*, int);
void proceso_salida(int*, struct ESTADO*, int);

void Sintaxis() {
    printf(
        "--------------------------------------------------------------------------------------\n"
        "Uso: ./parking <ficheroEntrada0> <ficheroEntrada1> <capacidadTotal>                     \n"
        "   ficheroEntrada0     : Fichero con matricula, llegada y salida para puerta 0.        \n"
        "   ficheroEntrada1     : Fichero con matricula, llegada y salida para puerta 1.        \n"
        "   capacidadTotal     : Entero > 0. Capacidad máxima del parking.                     \n"
        "--------------------------------------------------------------------------------------\n"
    );
}

int main(int argc, char *argv[]) {
    int hora_inicial, hora_final;
    int capacidad;
    struct ESTADO *estado = NULL;
    int p_sim_a_entrada[NUM_ENTRADAS][2];
    int p_sim_a_salida[2];
    pid_t pid_entrada[NUM_ENTRADAS];
    pid_t pid_salida;
    char *fichero_entrada[NUM_ENTRADAS];
    struct VEHICULO *accesos_entrada[NUM_ENTRADAS];
    int num_acceso_entrada[NUM_ENTRADAS];
    int num_llegadas_ciclo;
    int llegadas_ciclo[MAX_VEHICULOS];
    int es_hijo = 0;

    // --- Comprobación de parámetros de entrada ---------------------------- //
    // Este programa recibe 3 parámetros: <ficheroEntrada0> <ficheroEntrada1> <capacidadTotal>
    //
    //     ficheroEntrada0:
    //          Fichero (debe existir) que contiene 3 columnas con las llegadas
    //          a la entrada 0.
    //          Las columnas (separadas por tabuladores) son en ese orden:
    //              * la matricula,
    //              * la hora de llegada
    //              * la hora de salida.
    //          El fichero debe existir.
    //
    //     ficheroEntrada1:
    //          Fichero (debe existir) que contiene 3 columnas con las llegadas
    //          a la entrada 1.
    //          Las columnas (separadas por tabuladores) son en ese orden:
    //              * la matricula,
    //              * la hora de llegada
    //              * la hora de salida.
    //          El fichero debe existir.
    //
    //     capacidadTotal:
    //          Entero positivo que representa la capacidad máxima del parking.
    //
    //  No hay argumentos opcionales, todos son necesarios y no tienen valor
    //  por defecto.
    //
    //  En el caso de que el número de argumentos introducidos sea incorrecto
    //  o alguno de ellos no sea válido, el programa termina devolviendo el
    //  valor ERROR_SALIDA.

    // TAREA 1 - Validación argumentos de entrada

    // TAREA 1.a - Comprobamos que el número de parámetros sea correcto.
    if (argc != 4) {
        printf("ERROR: número incorrecto de parámetros.\n");
        Sintaxis();
        exit(ERROR_SALIDA);
    }

    // TAREA 1.b
    // <capacidadTotal> es enteros positivos no nulo.
    // Comprobamos que es numérico, lo almacenamos en la variable "capacidad"
    // de tipo int y comprobamos que es mayor que cero.
    if (!esNumero(argv[3])) {
        printf("ERROR: <capacidadTotal> debe ser entero no negativos.\n");
        Sintaxis();
        exit(ERROR_SALIDA);
    }

    capacidad = atoi(argv[3]);

    if (capacidad <= 0) {
        printf("ERROR: la capacidad total debe ser mayor que 0.\n");
        Sintaxis();
        exit(ERROR_SALIDA);
    }

    // --- Fin de Comprobación de parámetros de entrada --------------------- //

    // --- Inicializaciones y preparativos ---------------------------------- //

    // El primer paso es cargar las listas de vehículos que llegan a cada entrada.
    // Para ello leemos los ficheros y cargamos los datos con "procesar_archivo()".
    // Almacenamos los resultados en dos arrays:
    //      - accesos_entrada[NUM_ENTRADA=0|1][num_accesos]:
    //          array de array de estructuras VEHICULO, que contiene la
    //          información de cada linea de la tabla
    //      - num_acceso_entrada[NUM_ENTRADA=0|1]:
    //          array de int, almacena el número de vehículos que llegan a cada
    //          puerta.
    for (int i=0; i<NUM_ENTRADAS; i++) {
        fichero_entrada[i] = argv[1+i];

        if (0 >= (num_acceso_entrada[i] = procesar_archivo(
                            fichero_entrada[i],
                            &accesos_entrada[i]
         ))) {
            printf("ERROR: Fichero de puerta 1 incorrecto.\n");
            Sintaxis();
            exit(ERROR_SALIDA);
        }
    }

    // Calculamos el instante inicial de la simulación buscando el tiempo
    // mínimo de llegada entre los vehiculos que llegan a las 2 entradas.
    //
    // Inicializamos la hora_inicial al valor del primer vehículo de la
    // entrada 0.
    //
    // El orden de llegada de vehículos a una misma puerta está garantizado.
    // Solo comparamos con el primer vehículo de cada una de las puertas.
    hora_inicial = accesos_entrada[0][0].llegada;
    for (int i=0; i<NUM_ENTRADAS; i++) {
        if (accesos_entrada[i][0].llegada < hora_inicial) {
            hora_inicial = accesos_entrada[i][0].llegada;
        }
    }

    // De igual manera, calculamos el instante final de la simulación.
    //
    // Inicializamos la hora_final al valor de la hora_inicial
    //
    // En este caso comprobamos todos los vehículos porque podría pasar más
    // tiempo en el aparcamiento un coche que llegara entre los primeros.
    hora_final = hora_inicial;
    for (int j=0; j<NUM_ENTRADAS; j++) {
        for (int i=0; i<num_acceso_entrada[j]; i++) {
            if (accesos_entrada[j][i].salida > hora_final) {
                hora_final = accesos_entrada[j][i].salida;
            }
        }
    }
    // Añadimos un instante más para garantizar que salen todos los vehículos.
    // Es necesario para el caso de un acceso en el último ciclo previsto
    // (hora_final) por estar lleno el aparcamiento y con hora de salida
    // (hora_final).
    // Al acceder al aparcamiento en su hora de salida, necesita un ciclo más
    // para salir.
    hora_final++;

    // Reservamos la memoria compartida.
    //
    // La memoria compartida contendrá únicamente una variable cuyo tipo es una
    // estructura ESTADO.
    // Esta estructura almacena el estado global del parking.
    //
    // TAREA 2 - reserva de memoria compartida
    int shmId;
    if (-1 == (shmId = shmget(
                    ftok("./", SHMSEED),
                    sizeof(struct ESTADO),
                    IPC_CREAT | SHMPERMISOS
    ))) {
        perror("ERROR al crear la memoria compartida");
        exit(ERROR_SALIDA);
    }
    estado = (struct ESTADO*)shmat(shmId, 0, 0); // memoria no comprobada
    if (estado == (void*) -1) {
        perror("ERROR al mapear la memoria compartida.\n");
        exit(ERROR_SALIDA);
    }

    // Inicializamos la memoria compartida
    // La capacidad del apparcamiento se ha recibido por argumento
    // y la ocupacion del parking empieza siempre en 0.
    estado->capacidad = capacidad;
    estado->ocupacion = 0;

    // Definimos e inicializamos el semáforo que garantiza la exclusion mutua
    // durante el acceso a la memoria compartida.
    // TAREA 3 - Inicialización del semaforo
    int sMutex;
    sMutex = sCreate(SSEED, 1);

    // Inicialización de pipes pipes para la comunicación entre el padre
    // y los distintos hijos.
    // Definiremos un proceso hijo por entrada y uno para la salida
    // y se encargaran respectivamente de gestionar los accesos y salidas de
    // vehículos.
    //
    // TAREA 4 - Inicialización de pipes para comunicacion del padre a hijos.
    //      Los pipes permitirán al padre enviar:
    //          - la evolución del instante temporal actual a todos los hijos,
    //            consiguiendo así sincronizar los procesos.
    //          - los datos de cada vehículo que llega a una de las entradas,
    //            comunicado exclusivamente al proceso de entrada
    //            correspondiente.
    for (int i=0; i<NUM_ENTRADAS; i++) {
        if(-1 == pipe(p_sim_a_entrada[i])) {
            perror("ERROR al crear pipes");
            exit(ERROR_SALIDA);
        }
    }

    if (-1 == pipe(p_sim_a_salida)) {
        perror("ERROR al crear pipe_log");
        exit(ERROR_SALIDA);
    }

    // --- Fin de la sección de inicializaciones y preparativos ------------- //

    // --- Ejecución de procesos hijos -------------------------------------- //

    // TAREA 5 - Labores de inicio del proceso Salida
    // 5.a Lanzamos la ejecución del proceso Salida
    if (-1 == (pid_salida = fork())) {
        perror("ERROR en fork de salida");
        exit(ERROR_SALIDA);
    }

    // 5.b Ejecutamos la función correspondiente al proceso Salida
    if (pid_salida == 0) {
        proceso_salida(p_sim_a_salida, estado, sMutex);
        es_hijo = 1;
    }

    // TAREA 6 - Labores de inicio de los procesos Entrada
    // 6.a Lanzamos la ejecución de los procesos de Entradas
    for (int i=0; i<NUM_ENTRADAS; i++) {
        if (!es_hijo) {
            if (-1 == (pid_entrada[i] = fork())) {
                char msg[BUF_SIZE];
                snprintf(msg, sizeof(msg), "ERROR en fork de entrada %d.\n", i);
                perror(msg);
                exit(ERROR_SALIDA);
            }

    // 6.b Ejecutamos la función correspondiente a los procesos Entrada
            if (pid_entrada[i] == 0) {
                proceso_entrada(i, p_sim_a_entrada[i], estado, sMutex);
                es_hijo = 1;
            }
        }
    }

    // --- Fin de la ejecución de procesos hijos ---------------------------- //

    // --- Código exclusivo del padre --------------------------------------- //

    if (!es_hijo) {

        // cierra extremos de pipes no usados y lanza la simulación
	    // TAREA 7 - Cierre de pipes no utilizados por el padre
        for (int i=0; i<NUM_ENTRADAS; i++) {
             close(p_sim_a_entrada[i][0]);
        }
        close(p_sim_a_salida[0]);

        // Bucle de simulación de tiempo.
        // Un bucle hace avanzar el instante temporal de 1 en 1 partiendo de
        // hora_inicial y terminando en hora_final.
        for (int t=hora_inicial; t<=hora_final; t++) {
            // Imprimos el estado del aparcamiento
            imprimir_estado_aparcamiento(t, estado, sMutex);

	        // TAREA 8 - Envio de la marca (instante) temporal actual a cada
            //      proceso hijo via pipe.
            for (int i=0; i<NUM_ENTRADAS; i++) {
                write(p_sim_a_entrada[i][1], &t, sizeof(int));
            }
            write(p_sim_a_salida[1], &t, sizeof(int));

            // El proceso padre notifica a las entradas de los vehículos que
            // llegan en el instante actual.
            // Primero notifica de cuantos vehículos llegan a cada entrada
            // y después les envia los datos de esos vehículos.
            // Todos los envios se hacen mediante pipes.
            for (int j=0; j<NUM_ENTRADAS; j++) {
                num_llegadas_ciclo = 0;
                for (int i=0; i<num_acceso_entrada[j]; i++) {
                    if (accesos_entrada[j][i].llegada == t) {
                        llegadas_ciclo[num_llegadas_ciclo] = i;
                        num_llegadas_ciclo++;
                    }
                }
                // TAREA 9 - Envio via pipe del número de vehículos que llegan
                //      a cada entrada en ese instante.
                write(p_sim_a_entrada[j][1], &num_llegadas_ciclo, sizeof(int));

                // TAREA 10 - Como las entradas ya saben cuantos vehículos llegan,
                //      el proceso padre envía la información de cada uno
                //      de esos vehículos a la entrada correspondiente.
                //      El envio es de una estructura de tipo VEHICULO
                //      y se hace mediante pipe.
                for (int i=0; i<num_llegadas_ciclo; i++) {
                    write(
                        p_sim_a_entrada[j][1],
                        &accesos_entrada[j][llegadas_ciclo[i]],
                        sizeof(struct VEHICULO)
                    );
                }

            }

            // 0.1 segundos
            usleep(100000);
        }
        imprimir_estado_aparcamiento(hora_final+1, estado, sMutex);

        // Marcamos fin de simulación enviando el instante temporal -1 a todos
        // los procesos hijos.
	    // TAREA 11 - Notificación de fin de ejecución via pipe.
        hora_inicial = -1;
        for (int i=0; i<NUM_ENTRADAS; i++) {
            write(p_sim_a_entrada[i][1], &hora_inicial, sizeof(int));
        }
        write(p_sim_a_salida[1], &hora_inicial, sizeof(int));

	    // TAREA 12 - Cierre de los pipes restantes
        for (int i=0; i<NUM_ENTRADAS; i++) {
             close(p_sim_a_entrada[i][1]);
        }
        close(p_sim_a_salida[1]);

	    // TAREA 13 - Liberación de memoria compartida y destrucción del semáforo.
        shmdt(estado);
        shmctl(shmId, IPC_RMID, 0);
        sDestroy(sMutex);
    }

    // --- Fin del código exclusivo del padre ------------------------------- //

    // --- Limpieza final compartida ---------------------------------------- //

    // Liberamos la memoria reservada dinámicamente.
    for (int i=0; i<NUM_ENTRADAS; i++) {
         free(accesos_entrada[i]);
    }

    // El padre espera a que terminen los hijos.
    // TAREA 14 - Garantizar el fin de todos los procesos hijos
    for (int i=0; i<NUM_ENTRADAS+1; i++) {
        wait(NULL);
    }

    return 0;

    // --- Fin del programa ------------------------------------------------- //
}

// Devuelve 1 si str es un número entero no negativo.
int esNumero(char *str) {
    if (str == NULL || *str == '\0') {
        return 0;
    }
    for (size_t i=0; str[i]; ++i) {
        if (!isdigit((unsigned char)str[i])) return 0;
    }
    return 1;
}


// Lee un archivo correspondiente a una entrada y devuelve
//  - por referencia, un array de estructuras VEHICULO con la información de
//    cada llegad
//  - por valor, el número de vehículos en el array devuelto por referencia.
int procesar_archivo(char *archivo, struct VEHICULO **accesos) {
    char matricula[8];
    int hora_inicial, llegada, salida;

    FILE *fd;
    size_t i=0, N=0;

    // Medimos la longitud del archivo.
    if ((fd = fopen(archivo, "r")) == NULL) {
        return -1;
    }
    // Leemos linea a linea para conocer la longitud.
    while (fscanf(fd, "%7s %d %d", matricula, &llegada, &salida) == 3) {
        N++;
    }
    if (N <= 0) {
        fclose(fd);
        return -1;
    }

    // Retrocedemos de vuelta al principio del archivo.
    rewind(fd);

    // Reservamos memoria y la rellenamos con los datos del archivo.
    if ((*accesos = calloc(N, sizeof(struct VEHICULO))) == NULL) {
        return -1;
    }
    for (i=0; i<N; i++) {
        if (fscanf(fd, "%7s %d %d", matricula, &llegada, &salida) == 3) {
            // almacena hora inicial
            if (i==0) {
                hora_inicial = llegada;
            // comprueba que están listados en orden
            } else {
                if (llegada < hora_inicial) {
                    fclose(fd);
                    return -1;
                }
                hora_inicial = llegada;
            }
            // comprueba que un coche no sale antes o en la misma hora de llegada
            if (salida <= llegada) {
                fclose(fd);
                return -1;
            }
            strncpy((*accesos)[i].matricula, matricula, 8);
            (*accesos)[i].llegada = llegada;
            (*accesos)[i].salida = salida;
        }
    }

    fclose(fd);
    return i;

}

// Imprime el instante de tiempo y el estado de la ocupación del
// aparcamiento.
// Ej: "[T=3]   ocupacion=4/10."
void imprimir_estado_aparcamiento(int hora, struct ESTADO *estado, int sMutex) {
    printf("[T=%d]\t ", hora);
    fflush(stdout);
    // TAREA 15.a - Reservamos el acceso a la memoria compartida.
    sWait(sMutex);
    printf("ocupacion=%d/%d.\n", estado->ocupacion, estado->capacidad);
    fflush(stdout);
    // TAREA 15.b - Liberamos el acceso a la memoria compartida.
    sSignal(sMutex);
}


// Proceso de gestión de una puerta de entrada.
// Esta función realiza todas las tareas propias de un proceso entrada menos
// la limpieza común a todos los procesos.
// Tras cerrar los pipes innecesarios, entra en un bucle infinito.
// En ese bucle empieza por esperar via pipe el entero que indica el tiempo
// en esa iteración. Se espera que avance de 1 en 1.
// Después espera via pipe otro entero que indica cuantos vehículos quieren
// acceder al aparcamiento en ese instante.
// Entrarán en el orden de llegada mientras haya huecos disponibles.
// De no poder entrar, permanecerán en la cola hasta que, en algún tiempo
// posterior, quede un hueco libre en el aparcamiento.
// El bucle termina cuando recibe un instante temporal -1.
void proceso_entrada(
    int num_puerta,
    int *pipe_sim_a_puerta,
    struct ESTADO *estado, int sMutex
    ) {

    int hora, llegadas;
    int libre;
    int num_en_cola = 0;
    struct VEHICULO cola[MAX_VEHICULOS];
    int error = 0;

    // TAREA 16 - Cierre de extremos de pipes innecesarios.
    close(pipe_sim_a_puerta[1]);

    for (;;) {
        // TAREA 17 - Recibe via pipe el entero que representa la hora actual.
        read(pipe_sim_a_puerta[0], &hora, sizeof(int));
        // Comprobamos el final de la simulación.
        if (hora == -1){
            return;
        }
        // TAREA 18 - Recibe via pipe el número de coches que llegan a esta hora.
        read(pipe_sim_a_puerta[0], &llegadas, sizeof(int));
        for (int i=0; i<llegadas; i++) {
            // TAREA 19 - Recibe todos esos coches uno a uno (estructura VEHICULO).
            read(pipe_sim_a_puerta[0], &cola[num_en_cola], sizeof(struct VEHICULO));
            // Compruebamos que la hora de llegada almacenada en la estructura
            // es correcta (corresponde con el instante actual).
            if (cola[num_en_cola].llegada != hora) {
                printf("ERROR al procesar llegadas a la puerta %d.\n", num_puerta);
                fflush(stdout);
                error = 1;
                break;
            }
            num_en_cola++;
        }

        // En caso de llegada a una hora incorrecta, se procude una salida
        // anticipada del bucle infinito.
        if (error) {
            break;
        }

        // Procesamos la cola si el acceso es posible.
        // TAREA 20 - Reservamos el acceso a la memoria compartida.
        sWait(sMutex);
        // Calculamos cuantas plazas libres hay en el aparcamiento.
        libre = estado->capacidad - estado->ocupacion;
        // Mientras queden plazas libres, dejamos pasar vehículos uno a uno.
        while (libre > 0) {
            if (num_en_cola == 0) {
                break;
            }

            // Imprimimos información del nuevo acceso.
            printf(
                "[T=%d][Puerta %d]\t %s ha entrado al parking "
                    "- entrada prevista T=%d "
                    "- salida T=%d.\n",
                hora,
                num_puerta,
                cola[0].matricula,
                cola[0].llegada,
                cola[0].salida
            );

            // Actualizamos todos los contadores.
            num_en_cola--;
            libre--;
            estado->ocupacion++;

            // Introducimos el vehículo que acaba de entrar en la memoria
            // compartida que almacena el estado del aparcamiento.
            memcpy(
                &estado->plazas[estado->ocupacion-1],
                &cola[0],
                sizeof(struct VEHICULO)
            );

            // Retiramos el vehículo que ha entrado de la cola de acceso.
            // Estamos utilizando un array estático como cola, así que copiamos
            // todos los vehículos restantes al principio del array.
            // memmove, not memcpy: source and destination overlap.
            memmove(&cola[0], &cola[1], num_en_cola * sizeof(struct VEHICULO));

            // Un mensaje del estado del parking.
            printf(
                "[T=%d][Puerta %d]\t Ocupacion actualizada: %d/%d.\n",
                hora,
                num_puerta,
                estado->ocupacion,
                estado->capacidad
            );
            fflush(stdout);
        }
        // TAREA 21 - Liberamos el acceso a la memoria compartida.
        sSignal(sMutex);

        // 0.1 segundos
        usleep(100000);
    }

    // TAREA 22 - Cierre de los extremos de pipes restantes.
    close(pipe_sim_a_puerta[0]);
}

// Proceso de gestión de salida de vehiculos.
// Esta función realiza todas las tareas propias del proceso salida menos
// la limpieza común a todos los procesos.
void proceso_salida(
    int *pipe_sim_a_salida,
    struct ESTADO *estado, int sMutex
    ) {

    int hora;

    // TAREA 23 - Cierre de extremos de pipes innecesarios.
    close(pipe_sim_a_salida[1]);

    for (;;) {
        // TAREA 24 - Recibe via pipe el entero que representa la hora actual.
        read(pipe_sim_a_salida[0], &hora, sizeof(int));
        // Comprobamos el final de la simulación.
        if (hora == -1) {
            return;
        }

        // TAREA 25 - Reservamos el acceso al estado compartido del aparcamiento
        // para dar salida a los coches.
        sWait(sMutex);
        // Comprobamos uno a uno los vehículos aparcados para comprobar
        // si alguno quiere salir a esta hora.
        for (int i=0; i<estado->ocupacion; i++) {
            if (estado->plazas[i].salida <= hora) {
                // Salida detectada, lo imprimimos por pantalla.
                printf(
                    "[T=%d][Salida]\t %s ha salido del parking "
                        "- salida prevista T=%d.\n",
                    hora,
                    estado->plazas[i].matricula,
                    estado->plazas[i].salida
                );
                fflush(stdout);

                // Eliminamos el vehículo saliente de la memoria compartida que
                // mantiene el estado del aparcamiento.
                // Al tratarse de una cola implementada en un array,
                // desplazamos toda la memoria desde la posición siguiente a
                // ese vehículo, una posición hacia atrás.
                // memmove, not memcpy: source and destination overlap.
                memmove(
                    &estado->plazas[i],
                    &estado->plazas[i+1],
                    (estado->ocupacion-i-1)*sizeof(struct VEHICULO)
                );
                // Actualizamos los contadores.
                --estado->ocupacion;
                --i;
            }
        }
        // TAREA 26 - Liberamos el acceso a la memoria compartida.
        sSignal(sMutex);

        // 0.1 segundos
        usleep(100000);
    }

    // TAREA 27 - Cierre de los extremos de pipes restantes.
    close(pipe_sim_a_salida[0]);
}

