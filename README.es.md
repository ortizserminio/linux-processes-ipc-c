# Procesos e IPC en Linux con C: simulaciones de un aparcamiento

[English](README.md) · **Español**

[![CI](https://github.com/ortizserminio/linux-processes-ipc-c/actions/workflows/ci.yml/badge.svg)](https://github.com/ortizserminio/linux-processes-ipc-c/actions/workflows/ci.yml)

Dos programas multiproceso para Linux, escritos para los exámenes prácticos de *Sistemas Operativos* (Universidad Miguel Hernández, 2025/26). Los dos simulan un aparcamiento usando solo primitivas POSIX y System V: `fork`, `wait`, tuberías (pipes), memoria compartida (`shmget` / `shmat`) y semáforos (`semget` / `semop`). Un lanzador en Bash y unos tests de integración se ejecutan en CI sobre Ubuntu.

| Carpeta | Procesos | Qué demuestra |
|---|---|---|
| [`parking-two-gates/`](parking-two-gates) | 1 padre (reloj) + 2 puertas de entrada + 1 salida | Una simulación por instantes: el padre envía la hora por pipes y los hijos actualizan el estado compartido dentro de un mutex |
| [`parking-workers/`](parking-workers) | 1 padre (reparto) + 4 workers | **Orden entre procesos**: los eventos llegan en paralelo a distintos workers, pero deben aplicarse y registrarse en el orden global de secuencia |

## Examen 1: dos puertas de entrada y una salida (`parking-two-gates`)

```
                 pipe: t, n, VEHICULO...   ┌──────────────┐
            ┌─────────────────────────────▶│  entrada 0   │──┐
┌────────┐  │  pipe: t, n, VEHICULO...     ├──────────────┤  │   sWait / sSignal
│ padre  │──┼─────────────────────────────▶│  entrada 1   │──┼──▶ memoria compartida: struct ESTADO
│ (reloj)│  │  pipe: t                     ├──────────────┤  │    {capacidad, ocupación, coches[]}
└────────┘  └─────────────────────────────▶│    salida    │──┘
                                           └──────────────┘
```

1. El padre valida los argumentos y carga las llegadas de cada puerta (`matrícula  llegada  salida`). Crea un segmento de memoria compartida para el estado del aparcamiento, un semáforo que hace de **mutex** y una pipe por hijo, y después crea los hijos con `fork`.
2. **Reloj simulado:** en cada instante `t`, el padre escribe `t` en todas las pipes. Después escribe a cada puerta de entrada cuántos coches llegan en `t`, seguido de esos registros `struct VEHICULO`. Tras el último instante envía `-1`.
3. **Las puertas de entrada** ponen en cola los coches que reciben. Dentro del mutex, admiten coches mientras haya plazas libres y los añaden al array compartido.
4. **El proceso de salida**, dentro del mutex, retira cada coche cuya hora de salida ha llegado y compacta el array.
5. El padre cierra sus extremos de las pipes, desvincula y elimina la memoria compartida, destruye el semáforo y hace `wait` de cada hijo.

`lanzar_parking.sh` separa por puerta un fichero de entrada combinado, ejecuta el programa, guarda su salida en un log y calcula a partir del log cuántos instantes estuvo lleno el aparcamiento y el tiempo medio de espera para entrar.

## Examen 2: cuatro workers con orden global (`parking-workers`)

El padre lee un CSV de eventos (`segundo, prioridad, E|S, puerta, matrícula`) y los numera `seq = 1, 2, 3…`. Envía cada evento a uno de **4 procesos worker** (entrada/salida × puerta 0/1), cada uno por su propia pipe. Los workers se ejecutan en paralelo, pero el log debe listar los eventos exactamente en orden de `seq`. Se resuelve con memoria compartida y semáforos:

* `nextSeq`, en memoria compartida, es el siguiente evento que puede avanzar.
* Un worker toma el mutex y compara el `seq` de su evento con `*nextSeq`. Si no es su turno, marca su indicador `esperando[i]` (todavía con el mutex), suelta el mutex y se duerme en **su propio** semáforo `sTurno[i]`.
* El worker al que le toca actualiza el estado del aparcamiento, escribe su línea en el log CSV, incrementa `nextSeq` y, antes de soltar el mutex, **despierta a todos los workers que esperan**: borra cada indicador activo y hace signal sobre el semáforo de ese worker. Cada worker despertado vuelve a comprobar si es su turno.

El enunciado del examen pedía otra forma de despertar: hacer signal `NUM_WORKERS − 1` veces sobre un único semáforo compartido. Esa versión puede bloquearse (ver más abajo), así que se sustituyó después del examen.

El resultado debe coincidir línea a línea con el log de referencia del profesor, y los tests comprueban exactamente eso.

## Compilar y probar (Linux)

```bash
cd parking-two-gates && make && ./parking <puerta0.txt> <puerta1.txt> <capacidad>
cd parking-workers   && make && ./parking data/eventos_profesor.csv salida.csv

bash tests/test_parking_two_gates.sh
bash tests/test_parking_workers.sh
```

Los tests de integración ([`tests/`](tests)) comprueban:

* **Examen 1**, con capacidades 4 y 10:
  - cada coche entra y sale exactamente una vez;
  - la ocupación nunca supera la capacidad;
  - el aparcamiento acaba vacío;
  - **no se queda ningún segmento de memoria compartida ni semáforo sin liberar** (`ipcs` antes y después);
  - se rechazan los argumentos no válidos (número incorrecto, capacidad no numérica o cero, fichero inexistente, salida antes de la llegada, llegadas desordenadas);
  - el lanzador funciona de principio a fin y calcula bien las horas a plena capacidad.
* **Examen 2**:
  - cinco ejecuciones cuyo log debe ser **idéntico a la referencia del profesor** (la planificación de procesos cambia en cada una);
  - 200 eventos generados cuyo log debe salir en orden de secuencia 1..200;
  - una **prueba de estrés** de 30 ejecuciones, cada una con `timeout`, para que un interbloqueo haga fallar el build en vez de dejarlo colgado;
  - validación de argumentos.

El **CI** ([`.github/workflows/ci.yml`](.github/workflows/ci.yml)) ejecuta las dos baterías dos veces en Ubuntu, una con una build normal `-O2` y otra con **AddressSanitizer + UndefinedBehaviorSanitizer**, y pasa **ShellCheck** sobre los scripts.

## Bugs encontrados y corregidos después del examen

Cada corrección está en su propio commit:

* **Comportamiento indefinido en C:** la cola de entrada y el array compartido de coches aparcados se compactaban con `memcpy` desde el elemento `i+1` sobre el `i`. Origen y destino se solapan, y eso es indefinido para `memcpy` (AddressSanitizer lo detecta como `memcpy-param-overlap`). Se sustituyó por `memmove`.
* **Interbloqueo en el examen 2 (encontrado por el CI):** el protocolo de turnos del enunciado hace un "broadcast" haciendo signal `NUM_WORKERS − 1` veces sobre un semáforo compartido. Un worker al que no le toca puede despertarse, comprobar, volver a bloquearse y **robar una segunda señal**, dejando dormido para siempre al worker al que sí le toca. Depende de la planificación: nunca apareció en clase ni en la build con sanitizers, pero colgó la build `-O2` en los servidores Linux de GitHub. Se corrigió emulando una variable de condición: un semáforo por worker más un indicador `esperando[]` que se escribe y se lee dentro del mutex, así que un despertar no se puede perder ni robar. Ahora una prueba de estrés ejecuta 200 eventos 30 veces con `timeout`.
* **Métrica incorrecta en Bash:** "horas a plena capacidad" buscaba `4/4` en cualquier parte del log, incluidas las líneas `Ocupacion actualizada: 4/4` que se imprimen en cada entrada. Con el ejemplo del examen daba **6** horas en vez de **2** (de 13 instantes).
* **Robustez del lanzador:** no creaba su carpeta de salida, dividía por cero si no entraba ningún coche y escribía las líneas en blanco de la entrada en el fichero de la puerta 0 (en Bash, `[[ "" -eq 0 ]]` es verdadero). Esto último lo encontró el CI.

## Autoría del código

Eran exámenes con un esqueleto proporcionado por el profesor, así que parte de cada archivo no es mía:

| | Escrito por el profesor | Escrito por mí |
|---|---|---|
| `parking-two-gates/parking.c`, `lanzar_parking.sh` | Esqueleto, comentarios y funciones auxiliares | Todo lo que hay dentro de los bloques `TAREA n` |
| `parking-workers/parking.c` | Cabeceras, structs, funciones marcadas `NO TOCAR`, comentarios | Todo lo que hay entre `BEGIN-TODO-n` y `END-TODO`, más la corrección del interbloqueo (marcada *Post-exam fix*) |
| `sem.c`, `sem.h`, `makefile`, `data/` | Todo (librería de semáforos, fichero de compilación, datos de ejemplo y de referencia) | — |
| `tests/`, `.github/`, este README | — | Todo |

El primer commit contiene solo los archivos del profesor. Para el examen 1, `git diff 2080198 23c99a0` muestra exactamente las líneas que escribí yo. La [LICENCIA](LICENSE) (MIT) cubre solo mi código.

Los tests, el CI, el análisis de bugs y esta documentación se prepararon con ayuda de un asistente de IA. Las soluciones de los exámenes son trabajo mío.
