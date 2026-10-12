// parking.c
//
// Este es el fichero del código C de la evaluación.
// Contiene multiples bloques indicados con la palabra TODO que a su vez
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
#include <stdbool.h>  

#include "sem.h"

// Define el codigo error para exit()
#define ERROR 1

// Definir los códigos de error para la función PrintError()
#define ERR_NUM_PARAMETROS 0
#define ERR_NO_EVENTOS 1
#define ERR_FICHERO_LOG 2
#define ERR_PIPES 3
#define ERR_SHMID 4
#define ERR_SHMAT 5
#define ERR_LOGFILE 6
#define ERR_EVENTOSFILE 7
#define ERR_FORK 8

// Define las semillas y permisos para memoria compartida
#define SHMSEED 77
#define SHMPERMISOS 0644

// Define la semilla para semaforos
#define SSEED 200

// Define las caracteristicas del parking
#define CAPACIDAD 40
#define NUM_ENTRADAS 2 // Numero de barreras de entrada al parking
#define NUM_SALIDAS  2 // Numero de barreras de salida del parking

// Definimos el num caracteres de la matricula
#define LEN_MATRICULA 8

// Definimos la constantes de numero de workers
#define NUM_WORKERS 4

// Definimos el numero de File Descriptor de Lectura y Escritura para usarlo en los pipes 
#define FD_LECTURA 0
#define FD_ESCRITURA 1

const char *strCodes[]={
    "El número de parámetros es incorrecto",
    "El fichero de eventos no existe",
    "El fichero de log es incorrecto",
    "Problemas al crear los pipes",
    "Problemas al crear la memoria compartida",
    "Problemas al vincular con la memoria compartida",
    "No se puede crear el fichero de log",
    "No se puede abrir el fichero de eventos",
    "La llamada a fork() ha fallado"
};

// Esta estructura almacena el estado global del parking. Se reservará en
// memoria compartida para que los distintos procesos tengan acceso a ella.
struct ESTADO_PARKING {
    int capacidad;   // plazas totales
    int ocupacion;   // ocupación actual
    char plazas[CAPACIDAD][LEN_MATRICULA]; //array de plazas del parking. Cada plaza puede tener una matricula o ""
}; 

// Estructura que define un mensaje que se envía por los pipes
struct MENSAJE {
    int seq;    // Numero de secuencia de evento
    int seg;    // Segundo desde el comienzo del día en el que ocurre el evento
    char tipo;  // Tipo de evento 'E' o 'S'  (Entrada o Salida)
    int puerta; // Numero e puerta en donde procesar el evento
    char matricula[LEN_MATRICULA]; // Matricula del vehiculo que entra/sale
    int fin;    // 0 indica que vendran mas mensajes. 1 indica fin de los mensajes.
};

//--------- DECLARACIONES GLOBALES PARA QUE SE PUEDAN USAR EN CodigoWorkers ----------------------
// Declaramos global workerPipes porque en CodigoWorkers los usaremos
// Tenemos 4 workers (NUM_WORKERS), cada uno leera de un pipe en función del indice del worker de 0 a 3
// Declaramos un array de pipes para que cada worker acceda a su pipe por su indice
int workerPipes[NUM_WORKERS][2]; // fd[0] : lectura.  fd[1] escritura

// Declaramos los descriptores de los semaforos globales, para poder usarlos en CodigoWorker. Hay que crearlos en el main
int sMutex;
int sCheckTurno;

// Declaramos los punteros a memoria compartida como globales, pero hay que vincularlos en el Main.
struct ESTADO_PARKING *estadoParking = NULL;
int *nextSeq = NULL;

FILE *fdLogFile; //Descriptor de fichero para el fichero de log.
//-------------------------------------------------------------------------------------------------


// Declaraciones globales solo para poder hacer logging multiproceso en stdout.
int sConsole;
char consoleBuffer[1024]; //Para usar en sprintf para logs por consola. 

//---------------------------------------------------------------------------------------------
// FUNCIONES DE UTILIDDAD PARA USAR.     NO TOCAR.   USARLAS COMO ESTAN
//---------------------------------------------------------------------------------------------

// Funcion a llamar cuando se detecta un error en los parámetros
void Sintaxis(void) {
    printf(
        "------------------------------------------------------------------------------------------\n"
        "Uso: ./parking <ficheroEventos> <ficheroLog>                                              \n"
        "   ficheroEventos     : Nombre del fichero con los eventoes de entrada y salida al parking\n"
        "   ficheroLog         : Nombre del fichero donde generar los logs                         \n"
        "------------------------------------------------------------------------------------------\n"
    );
}

// Función para imprimir un mensaje de error en función del codigo recibido (definidos arriba)
void PrintError(int errCode){
    printf("\n*****************************************************\n");
    printf("ERROR: %s\n",strCodes[errCode]);
    printf("*****************************************************\n");
}

// Funcion para mostrar un MENSAJE por pantalla protegiendo la salida.
// Util para multiples procesos imprimiendo
// Podemos usarla para debuguear el código.
void PrintMensaje(int numProc, struct MENSAJE msg){
    sWait(sConsole);
        printf("MENSAJE by %d: seq[%3d] seg[%5d] tipo[%c] puerta[%1d] matricula[%8s] fin[%1d]\n", 
             numProc, msg.seq, msg.seg, msg.tipo, msg.puerta, msg.matricula, msg.fin);            
        fflush(stdout);
    sSignal(sConsole);
}

// Funcion para mostrar estadoParking por pantalla protegiendo la salida.
// Util para multiples procesos imprimiendo
// Podemos usarla para debuguear el código.
// Los workers pasan su numero [0..3] el padre puede pasar 999
void PrintEstado(int numProc){
    sWait(sConsole);
        printf("ESTADO by %d: capacidad[%3d] ocupacion[%3d]\n", 
               numProc, estadoParking->capacidad, estadoParking->ocupacion);    
        printf("       Plazas [");            
        for (int i=0; i<CAPACIDAD; i++){
            if (strlen(estadoParking->plazas[i])==0){
                printf(".");
            }
            else{
                printf("X");
            }
        }            
        printf("] NextSeq = %d\n", (*nextSeq));
        fflush(stdout);
    sSignal(sConsole);
}

// Funcion para mostrar un texto por pantalla protegiendo la salida.
// Util para multiples procesos imprimiendo
// Podemos usarla para debuguear el código.
void PrintConsole(char *texto){
    sWait(sConsole);
        printf("%s\n", texto);
        fflush(stdout);
    sSignal(sConsole);
}

// Función que comprueba si el nombre del fichero de log es valido.
bool EsNombreLogValido(const char *nombre_fichero) {
    int len = strlen(nombre_fichero);
    const char *extension = ".csv";
    int ext_len = strlen(extension);

    // 1. Verificar si la longitud es suficiente para una extensión .csv (mínimo 5 caracteres)
    if (len < ext_len) {
        return false; // No puede ser .csv
    }

    // 2. Comprobar si termina en .csv
    // strncmp compara los primeros 'ext_len' caracteres
    if (strncmp(nombre_fichero + len - ext_len, extension, ext_len) != 0) {
        return false; // No termina en .csv
    }

    // 3. Recorrer el nombre para buscar espacios en cualquier parte
    for (int i = 0; i < len; i++) {
        // isspace() devuelve verdadero (distinto de 0) si es un espacio, tab, etc.
        if (isspace((unsigned char)nombre_fichero[i])) {
            return false; // Encontró un espacio, no es válido
        }
    }

    // 4. Comprobar que no sea solo ".csv" (debe tener nombre antes del punto)
    if (len == ext_len) {
        return false; // El nombre es solo ".csv"
    }

    // Si pasó todas las pruebas
    return true;
}

// Funcion que devuelve true si existe un fichero o false en caso contrario
bool ExisteFichero(const char *nombreFichero) {
    FILE *archivo = fopen(nombreFichero, "r"); // Intentar abrir en modo lectura "r"
    if (archivo != NULL) {
        fclose(archivo); // Si se abrió, cerrarlo y devolver true
        return true;
    } else {
        return false; // Si es NULL, no existe o no se pudo abrir
    }
}
//---------------------------------------------------------------------------------------------
// FUNCIONES PARA EL CODIGO DE LOS WORKERS. HAY QUE COMPLETARLA
//---------------------------------------------------------------------------------------------
// Función común para todos los workers
// En funcion de los parametros sabe su tipo, su puerta y el indice del array workerPipes donde está su pipe
// El worker trabajará en bucle hasta que el mensaje que lea de su pipe sea el de FIN. Saldrá del bucle y terminara la funcíon. 
void CodigoWorker(int idxPipe){

    // Soy un worker

    //begin-Log a consola. Así podeis poner logs en consola que no se crucen con los de otros workers ni el padre.
    sprintf(consoleBuffer,"Worker %d : starts",idxPipe);
    PrintConsole(consoleBuffer);
    //end-log a consola

    // Declaramos la variable mensaje para leerla del pipe
    struct MENSAJE msg; 

    // Entramos en un bucle sin fin, del que saldremos con break al detectar el mensaje de FIN.
    while(true){
        // BEGIN-TODO-1 ******************************************************************
            // Leemos del Pipe correspondiente el mensaje.
            read(workerPipes[idxPipe][FD_LECTURA], &msg, sizeof(struct MENSAJE));

            // Detección si es mensaje de FIN y salir del bucle
            if (msg.fin == 1){
                break; 
            }
        // END-TODO **********************************************************************

        // BEGIN-TODO-2 ******************************************************************
            // Tanto para acceder a estadoParking como a nextSeq debemos proteger dichas variables compartidas con mutex
            // Rercordar que tanto estadoParking como nextSeq son punteros.
            // En bucle, comprobamos si es mi turno comparando mi seq recibida con nextSeq. Sera mi turno cuando 
            // habiendo leido seq del mensaje coincide con nextSeq en memoria compartida. Recordar que nextSeq es un puntero (int *)
            //    si NO es mi turno: Libero el mutex pero me cuelgo en el semaforo sCheckTurno. 
            //                       Cuando me libere un broadcast adquiero de nuevo el mutex y compruebo de nuevo el turno. 
            //    si SI es mi turno: Salgo del bucle, manteniendo el mutex, actualizo el estadoParking, incremento el nextSeq e imprimo en el log

            // Intentamos acceder a Memoria compartida con sMutex
            sWait(sMutex); 

            // Comienzo el bucle de comprobación y espera. 
            while( msg.seq != (*nextSeq) ){
                // liberamos el mutex a memoria compartida
                sSignal(sMutex); 
                // Me cuelgo a la espera de ser liberado por el broadcast en el semáforo sCheckTurno
                sWait(sCheckTurno); 
                // Me han liberado y voy a intentar obtener de nuevo el munex para volver al while donde compruebo de nuevo si es mi turno
                sWait(sMutex); 
            }
        // END-TODO **********************************************************************


        // BEGIN-TODO-3 ******************************************************************
            // Aquí ya se que es mi turno. Pero sigo dentro del sMutex.
            // Voy a actualizar estadoParking en memoria compartida
            // Si el mensaje es de entrada 'E'
            if (msg.tipo == 'E'){ 
                // Se produce una entrada, me da igual la puerta para incrementar en el estado la ocupacion.
                // Incremento la ocupación del estado. Recordar que estadoParking es un puntero
                estadoParking->ocupacion++;

                // Ahora asigno la primera plaza libre al coche que entra. 
                // Por tanto itero por las plazas hasta encontrar una libre, en la que aisgno su matricula copiando la matricula del mensaje.
                for (int idxPlaza=0; idxPlaza<estadoParking->capacidad; idxPlaza++){
                    // Si la plaza tiene una matricula "" (longitud 0) la hemos encontrado y la asignamos.
                    if (strlen(estadoParking->plazas[idxPlaza])==0){
                        strcpy(estadoParking->plazas[idxPlaza], msg.matricula);
                        break;
                    }
                }
            }
            else {
                // Se produce una salida, me ad igual la puerta para decrementar en el estado la ocupación
                // Decremento la ocupacion del estado. Recordar que estadoParking es un puntero
                estadoParking->ocupacion--;

                // Ahora busco la plaza que ocupaba y la libero asignandole ""
                // Por tanto itero por las plazas hasta encontrarla con strcmp asignandole ""
                for (int idxPlaza=0; idxPlaza<estadoParking->capacidad; idxPlaza++){
                    // Si la plaza tiene una matricula buscada, la libero copiandole "" como matricula
                    if (strcmp(estadoParking->plazas[idxPlaza], msg.matricula)==0){
                        strcpy(estadoParking->plazas[idxPlaza], "");
                        break;
                    }
                }
            }
        // END-TODO **********************************************************************

        // BEGIN-TODO-4 ******************************************************************
            // Ya hemos actualizado el estado el parking, ahora hay que actualizar el nextSeq
            // Incrementamos nextSeq en 1. Tener en cuenta que nextSeq es un puntero a entero (int *)
            (*nextSeq)++;

            //begin-log Mostramos por consola el estado y el siguiente seq number.
            PrintEstado(idxPipe); //El parametro sirve para decir quien muestra el estado.   
            //end-log
                    
            // Seguimos estando bajo ej sMutex
            // Obtenemos las plazas libres para poder sacalas en el fichero de log.
            int plazasLibres = estadoParking->capacidad - estadoParking->ocupacion; 

            //Escribimos en el fichero de log, via su descriptor de fichero fdLogFile, que el worker heredo en su creación.
            //Utilizar fflush para que se escriba inmediatamente en el fichero.
            fprintf(fdLogFile, "%d,%d,%c,%d,%s,%d,%d,%s\n",
                                msg.seq, msg.seg, msg.tipo, msg.puerta, msg.matricula, plazasLibres, estadoParking->ocupacion, "OK");
            fflush(fdLogFile); 

            // El worker libera el sMutex de acceso a memoria compartida
            sSignal(sMutex);
        // END-TODO **********************************************************************

        // BEGIN-TODO-5 ******************************************************************
            // Ya hemos actualizado el estado y logueado el evento.
            // Ahora liberamos con un "broadcast" a los workers (haciendo tantos signals como otros workers hay).
            // Asi, aquel que tenga el seq == nextSeq continuará, y el resto se volverá a bloquear a la espera de su turno.
            for (int i=0; i<NUM_WORKERS-1; i++){
                sSignal(sCheckTurno);
            }
        // END-TODO **********************************************************************
    } 
}


// Function MAIN----------------------------------------------------------------------------------
int main(int argc, char *argv[]) {

    // Creamos un semáforo que usaremos para sacar mensajes por consola por varios procesos, protegiendo la consola mientras cada proceso imprime.
    // sConsole lo hemos definido arriba como global para que pueda ser usado en otras funciones.
    sConsole = sCreate(SSEED+9, 1);

    //---------------------------------------------------------------------------------------------
    // LECTURA Y VALIDACION DE PARAMETROS
    //---------------------------------------------------------------------------------------------

    // BEGIN-TODO-6 ******************************************************************
        // Realizamos las comprobaciones y
        // Si hay error  llamamos a la función PrintError pasándole el código de error correspondiente (definidos arriba)
        // Llamamos también a Sintaxis() y terminamos con exit(ERROR);
        
        // Comprobamos que el núemro de parametros es correcto
        if (argc != 3) {
            PrintError(ERR_NUM_PARAMETROS);
            Sintaxis();
            exit(ERROR);
        }

        // Comprobamos que el parámetro <ficheroEventos> es un fichero, existe.
        if (!ExisteFichero(argv[1])){
            PrintError(ERR_NO_EVENTOS);
            Sintaxis();
            exit(ERROR);
        }

        // Comprobamos que el parámetro <ficheroLog> es un Nombre de Log Valido usando la función EsNombreLogValido() definida arriba.
        if (!EsNombreLogValido(argv[2])){
            PrintError(ERR_FICHERO_LOG);
            Sintaxis();
            exit(ERROR);
        }
    // END-TODO **********************************************************************

    // Si todo es correcto asignamos las variable para los ficheros que se usaran en el código.
    const char *eventosPath=argv[1];
    char *logPath = logPath=argv[2]; // Para el nombre del fichero de log.

    //begin-log
    printf("Padre: Control Parametros: eventosPath[%s] logPath[%s]\n", eventosPath, logPath);
    //end-log

    //---------------------------------------------------------------------------------------------
    // CREACION DE LOS PIPES
    //---------------------------------------------------------------------------------------------

    // BEGIN-TODO-7 ******************************************************************
        // Creamos los NUM_WORKER pipes en el array de workerPipes
        // Si no se puede crear algun pipe usamos PrintError() con su codigo de error y salimos con exit(ERROR)
        for (int i=0; i<NUM_WORKERS; i++) {
            if(pipe(workerPipes[i]) == -1) {
                PrintError(ERR_PIPES);
                exit(ERROR);
            }
        }
    // END-TODO **********************************************************************

    //begin-log
    printf("Padre: Pipes Creados\n");
    //end-log

    //---------------------------------------------------------------------------------------------
    // CREACION DE MEMORIA COMPARTIDA 
    //---------------------------------------------------------------------------------------------

    // Creamos dos segmentos de memoria compartida
    // - Uno para estadoParking (estructura que mantiene el estado del parking)
    // - Otro para nextSeq      (proxima secuencia a procesar al log)

    // BEGIN-TODO-8 ******************************************************************
        // Declaramos el id de memoria compartida para albergar la estrutura ESTADO_PARKING y comprobamos si se ha podido crear, 
        // dando error en caso contrario (usar PrintError)
        int shmIdEstado = shmget( ftok("./", SHMSEED), sizeof(struct ESTADO_PARKING), IPC_CREAT | SHMPERMISOS );
        if (shmIdEstado == -1 ) {
            PrintError(ERR_SHMID);
            exit(ERROR);
        }

        // Nos vincualamos (attach) al segmento shmIdEstado en la variable estadoParking (puntero) que es un puntero al segmento (al struct)
        // Si error usamos PrintError()
        estadoParking = (struct ESTADO_PARKING*) shmat(shmIdEstado, 0, 0); 
        if (estadoParking == (void*) -1) {
            PrintError(ERR_SHMAT);
            exit(ERROR);
        }

        // Declaramos el id de memoria compartida para albergar el valor de siguiente secuencia, que es un entero. Comprobamos creación y mensaje error si procede (usar PrintError)
        int shmIdNextSeq = shmget( ftok("./", SHMSEED+1), sizeof(int), IPC_CREAT | SHMPERMISOS );
        if (shmIdNextSeq == -1 ) {
            PrintError(ERR_SHMID);
            exit(ERROR);
        }

        // Nos vincualamos (attach) al segmento shmIdEstado en la variable estadoParking, que es un puntero al entero.
        // Si error usamos PrintError()
        nextSeq = (int *) shmat(shmIdNextSeq, 0, 0); 
        if (nextSeq == (void*) -1) {
            PrintError(ERR_SHMAT);
            exit(ERROR);
        }
    // END-TODO **********************************************************************
    
    // BEGIN-TODO-9 ******************************************************************
        // Inicializamos la memoria compartida para reflejar el estado inicial del parking en el struct compartido
        // Capacidad inicial es CAPACIDAD, Ocupación inicial es 0, y las plazas (que almacenan la matricula) están sin vehiculo por tanto copiamos "" en todas.
        estadoParking->capacidad=CAPACIDAD;
        estadoParking->ocupacion=0;
        for (int i=0; i<CAPACIDAD; i++){
            strcpy(estadoParking->plazas[i],"");
        } 

        // Inicializamos la variable compartida nextSeq con valor 1 (siguiente evento (seq) a procesar). tener en cuenta que nextSeq es un puntero (int *)
        *nextSeq = 1; 
    // END-TODO **********************************************************************

    //begin-log
    printf("Padre: Memoria Compartida Creada y Vinculada\n");
    PrintEstado(999);
    //end-log


    //---------------------------------------------------------------------------------------------
    // CREACION DE SEMAFOROS
    //---------------------------------------------------------------------------------------------

    // Usaremos dos semáforos, Uno para acceder al estado del parking y otro para bloquear al worker si su seq no es la siguiente a procesar. 
    // Utilizar semillas distintas para los semaforos.

    // BEGIN-TODO-10 ******************************************************************
        // Creamos el semáforo sMutex para acceder al estado del parking con su inicialización correcta.
        sMutex = sCreate(SSEED, 1);

        // Creamos el semaforo de bloqueo sCheckTurno para bloquear al worker en espera para comprobar si es su turno.
        // Lo inicializamos de forma correcta para que surta efecto el bloqueo.
        sCheckTurno = sCreate(SSEED+1, 0);
    // END-TODO **********************************************************************

    //---------------------------------------------------------------------------------------------
    // APERTURA DEL FICHERO DE LOG 
    //---------------------------------------------------------------------------------------------

    // BEGIN-TODO-11 ******************************************************************
        // Los workers deberán escribir en el log (path en logPath), abrimos el fichero para que lo hereden ya abierto.
        // usar la variable fdLogFile para el descriptor de fichero.
        // comprobar que la apertura no ha fallado, en su caso mostrar error con PrintError()
        fdLogFile = fopen(logPath, "w");
    
        if (fdLogFile == NULL) {
            // Si entramos aquí, la apertura falló
            PrintError(ERR_LOGFILE);
            exit(ERROR); 
        }
    // END-TODO **********************************************************************


    //---------------------------------------------------------------------------------------------
    // CREACION DE WORKERS.  CREACION DE PROCESOS HIJO.
    //---------------------------------------------------------------------------------------------

    // El código que debe ejecutar el Worker es el mismo independientemente de su tipo y puerta.
    // Cada worker recibirá mensjes solo de un tipo y puerta.
    // El codigo del worker es la función CodigoWorker(int idxPipe) que recibe su indice de lectura en workerPipes como parametro

    int pid; // para el pid y ver si fork ha funcionado

    // BEGIN-TODO-12 ******************************************************************
        // Creamos 4 workers en bucle, el indice del bucle será el indice de acceso al workerPipes para cada worker, por lo que se pasa a CodigoWorker.
        for (int i=0; i<NUM_WORKERS; i++){
            // Creamos al worker con fork() y comprobamos que fork() funcionó. Si error usamos PrintError() y salimos con exit(ERROR)
            pid = fork();
            if (pid == -1 ){
                PrintError(ERR_FORK);
                exit(ERROR);
            }

            //Tras el fork() comprobamos si es código del hijo. Entonces llamamos a CodigoWorker pasando su indice de acceso al pipe.
            //Tras la función, llamará a exit(0) para no continuar el bucle de creación de workers.
            if (pid == 0 ){
                CodigoWorker(i);
                exit(0); // Cada Worker cuando termine su función, hará exit.
            }

            // Sólo el padre continua en bucle.
        }
    // END-TODO **********************************************************************

    // A partir de aquí sólo sigue el padre, todos los workers hicieron exit()

    //---------------------------------------------------------------------------------------------
    // CIERRE DE LOS PIPES DE LECTURA NO USADOS POR EL PADRE.
    //---------------------------------------------------------------------------------------------

    // BEGIN-TODO-12 ******************************************************************
        //Una vez creados los workers (hijos), el padre no va a leer de los pipes de éstos, por lo que puede cerrar sus descriptores de lectura (FD_LECTURA)
        //Recordar que tenemos los pipes en un array workerPipes
        for (int i=0; i<NUM_WORKERS; i++) {
            close(workerPipes[i][FD_LECTURA]);
        }
    // END-TODO **********************************************************************

    //---------------------------------------------------------------------------------------------
    // LECTURA DEL FICHERO EVENTOS, TOKENIZACION Y ENVIO A PIPES DE MENSAJE CON EVENTO
    //---------------------------------------------------------------------------------------------

    // El padre abre el fichero de eventos (path en eventosPath) para ir leyendo por líneas y enviando al worker correspondiente el evento.
    // Hay que comprobar que el fichero se puede abrir en solo lectura (su existencia ya se comprobo en el control de parametros)

    // Declaramos el descriptor del fichero de eventos.
    FILE *fdEventos;

    // BEGIN-TODO-12 ******************************************************************
        // Abrimos el fichero en lectura. Si error usamos PrintError para indicarlo y salimos con exit(ERROR)
        fdEventos = fopen(eventosPath,"r");
        if (fdEventos == NULL ){
            PrintError(ERR_EVENTOSFILE);
            exit(ERROR);
        }
    // END-TODO **********************************************************************

    //leemos linea a linea el fichero. 
    char linea[500]; //Buffer para almacenar cada linea leida
    struct MENSAJE msg; // Creamos un mensaje que cargaremos con cada linea y enviaremos al pipe.

    // Usaremos strtok para tokenizar la linea leida.
    // Definimos las variables str que alojarán el resultado de cada strtok (cada campo leido de una linea del fichero.)
    char *strSeq, *strSeg, *strTipo, *strPuerta, *strFin; 

    //Inicializamos el contador de sequencia de eventos
    int seq=1;
    
    // BEGIN-TODO-13 ******************************************************************
        //Bucle de lectrua de lineas y tokenización
            //Para cada línea se debe:
            //  Eliminar el \n de la linea leida.
            //  Extraer los campos usando strtok. Recordar que el caracter separador es la coma ','.  
            //  Asignar al mensaje sus valores corresondientes adaptando o convieritendo el token si es necesario al tipo de su campo en msg.
            //  Aumentar el contador para la siguiente iteración
            //  Determinar el destinatario del mensaje (del evento), y enviarlo (escribirlo) a su pipe
        while (fgets(linea, sizeof(linea), fdEventos)) {
            //Eliminamos el caracter \n de la línea leida asignando \0 en la posición del \n.
            int posicion = strcspn(linea,"\n");
            linea[posicion]='\0';

            // La linea contiene los campos: seg, prioridad, tipo, puerta, matricula
            // Al mensaje msg que enviarmos por el pipe vama a añadir los campos seq y fin y no ponemos prioridad, 
            // El mensaje quedaría con los campos:  seq, seg, tipo, puerta, matricula, fin 
            char *tmp; // temporal

            //Tokenizamos y creamos el mensaje que se enviará al pipe
            //Cargamos la estructura en la variable msg con los tokens adaptados convenientemente
            //La prioridad no nos sirve en la parte C, se usó en Bash para poder ordenar bien el fichero de eventos, pero hay que tokenizarla igualmente
            //aunque no vaya en el mensaje.

            // Asignamos el numero de secuencia al mensaje
            msg.seq = seq; 
            // Obtenemos el primer token, lo convertimos a entero y lo asignamos al campo seg del mensaje.
            msg.seg = atoi(strtok(linea,",")); 
            // Obtenemos el siguiente token pero lo descartamos, es la prioridad, lo asignamos a tmp.
            tmp = strtok(NULL,","); 
            // Obtenemos el siguiente token y lo guardamos en tmp para poder extraer su primer caracter
            // que lo asignaremos al campo tipo del mensaje.
            tmp = strtok(NULL,","); 
            msg.tipo = tmp[0];   

            // Obtenemos el siguiente token y lo convertimos a entero y asignamos a puerta.
            msg.puerta = atoi(strtok(NULL,",")); 
            // Obtenemos el siguiente token y lo copiamos en el campo matricula
            strcpy(msg.matricula,strtok(NULL,",")); // token matricula
            // Asignamos 0 al campo fin. Sólo cuando se hayan enviado todos se pondra aquí un 1.
            msg.fin = 0; // Añadimos el campo fin.

            //Incrementamos la sequencia
            seq++;

            // Ahora que tenemos en msg el evento completo, podemos ver para quien es y determinar el indice del array de pipes en el que hay que escrbir.
            // En función de para quien sea, asignamos la variable idx con el índice del pipe correspondiente 
            int idx; //para almacenar el indice del pipe al que enviar
            // Si el tipo es 'E' ponemos idx a 0 o 1 en función de la puerta.
            if (msg.tipo == 'E') {
                if (msg.puerta == 0){
                    idx=0;
                }        
                else{
                    idx=1;
                }
            }
            // Si el tipo es 'S' ponemos idx a 2 o 3 en función de la puerta.
            else {
                if (msg.puerta == 0) {
                    idx=2;
                }
                else{
                    idx=3;
                }
            }
            //Usando el idx asignado escribimos el msg en el pipe en su descriptor de escritura.
            write(workerPipes[idx][FD_ESCRITURA], &msg, sizeof(struct MENSAJE));
        } // fin del while
    // END-TODO **********************************************************************

    //---------------------------------------------------------------------------------------------
    // ENVIO MENSAJE DE FIN
    //---------------------------------------------------------------------------------------------

    // Construimos un mensaje de fin, con valores fake en todos los campos excepto en el campo msg.fin que pondremos a 1
    msg.seq = 0; 
    msg.seg = 0;
    msg.tipo = '0';
    msg.puerta = 0;
    strcpy(msg.matricula, "");
    msg.fin = 1; // Añadimos el campo fin a 1.

    // BEGIN-TODO-14 ******************************************************************
        // Enviamos el mensaje de fin a todos los pipes
        for (int i=0; i<NUM_WORKERS; i++) {
            write(workerPipes[i][FD_ESCRITURA], &msg, sizeof(struct MENSAJE));
        }
    // END-TODO **********************************************************************

    //---------------------------------------------------------------------------------------------
    // CIERRE DE FICHERO EVENTOS Y PIPES ESCRITURA
    //---------------------------------------------------------------------------------------------

    // BEGIN-TODO-15 ******************************************************************
        // El padre cierra el fichero de eventos 
        fclose(fdEventos);

        // El padre ya no va a enviar más mensajes, cierra todos sus pipes de escritura
        for (int i=0; i<NUM_WORKERS; i++) {
            close(workerPipes[i][FD_ESCRITURA]);
        }
    // END-TODO **********************************************************************


    //---------------------------------------------------------------------------------------------
    // ESPERA TERMINACION WORKERS
    //---------------------------------------------------------------------------------------------

    // BEGIN-TODO-16 ******************************************************************
        // El padre espera a que terminen los workers.
        for (int i=0; i<NUM_WORKERS; i++) {
            wait(NULL);
        }
    // END-TODO **********************************************************************


    //---------------------------------------------------------------------------------------------
    // LIMPIEZA DE MEMORIA COMPARTIDA, DESTRUCCION DE SEMAFOROS, CIERRE DE LOG.     FIN DEL PADRE
    //---------------------------------------------------------------------------------------------

    // BEGIN-TODO-17 ******************************************************************
        // El padre cierra el fichero de log
        fclose(fdLogFile);

        // Desvinculamos la variable estado del parking (estadoParking) y destruimos el segmento de memoria (shmIdEstado)
        shmdt(estadoParking);
        shmctl(shmIdEstado, IPC_RMID, 0);
        
        // Desvinculamos la variable de secuencia (nextSeq) y destruimos el segmento de memoria (shmIdNextSeq)
        shmdt(nextSeq);
        shmctl(shmIdNextSeq, IPC_RMID, 0);
        
        //Destruimos los semáforos sMutex y sCheckTurno
        sDestroy(sMutex);
        sDestroy(sCheckTurno);
    // END-TODO **********************************************************************

    //Destruimos el semaforor de consola
    sDestroy(sConsole);

} // fin del Main, fin de Padre

