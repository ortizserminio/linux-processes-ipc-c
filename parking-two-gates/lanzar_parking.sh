#!/bin/bash

# Este es el fichero bash de la evaluación.
# Contiene multiples bloques indicados con la palabra TAREA
# que pueden contener uno o varios segmentos de código indicados con:
# ...
# Se trata de una o más lineas de código a completar.
# Se valorará la funcionalidad, limpieza y eficiencia del código.

#-------------------------------------------------
# Number()
#   Comprueba si una cadena es un número entero.
# Salida:
#   0 si es numérico
#   1 si no lo es
#-------------------------------------------------
Number() {
    if [ "$1" -eq "$1" ] 2>/dev/null; then
        return 0
    else
        return 1
    fi
}

#-------------------------------------------------
# Sintaxis()
#   Muestra la sintaxis del script.
#-------------------------------------------------
Sintaxis() {
    echo "-------------------------------------------------"
    echo "Uso: $0 <ficheroCoches> <capacidadTotal> <ficheroLog>"
    echo "   ficheroCoches     : Fichero con 4 columnas separadas por tabuladores:"
    echo "                       matricula, puerta, tiempo_llegada, tiempo_salida"
    echo "   capacidadTotal    : Entero > 0. Capacidad máxima del parking."
    echo "   ficheroLog        : Fichero donde se almacenará la salida del programa C."
    echo "-------------------------------------------------"
}

#-------------------------------------------------
# CheckArguments()
#   Valida los argumentos de este script.
#-------------------------------------------------
CheckArguments() {
#----------------------------------
# TAREA 1
# Implementación de la funcion CheckArguments para comprobar de argumentos.
# Si los argumentos son invalidos, la funcion debe:
#   - imprimir un mensaje de error
#   - imprimir la sintaxis
#   - salir con código de error 1.
#----------------------------------

    # Comprobamos que el número de parámetros es 3.
    ...

    # Comprobamos que el primer parámetro es un fichero existente.
    ...

    # Comprobamos que el segundo parámetro es numérico.
    ...

    # Comprobamos rangos lógicos: el argumento numérico es estrictamente
    # mayor que 0.
    ...
}

# Programa principal

#----------------------------------
# TAREA 2
# Comprobación de argumentos y asignacion de variables.
#----------------------------------
# Comprobamos los argumentos con la función previamente implementada
...

# Asignamos los argumentos de entrada a variables mas cómodas
ficheroCoches=...
capacidad=...
ficheroLog=...

# Definimos los archivos de salida para cada puerta de entrada
archivoEntrada0="accesos/entrada_0.txt"
archivoEntrada1="accesos/entrada_1.txt"

# Separamos el archivo de coches en función de la puerta de acceso.
# El archivo original tiene 4 columnas: matricula, puerta, t_llegada, t_salida
# Generamos dos archivos con 3 columnas: matricula, t_llegada, t_salida
# Tanto en el archivo original como los archivos resultantes, las columnas estan
# separadas por tabuladores.
#----------------------------------
# TAREA 3
# Procesar el archivo de accesos ($archivoCoche) y generar los archivos
# de cada entrada ($archivoEntrada0 y $archivoEntrada1).
#----------------------------------
...

# Lanzamos el programa en C (parking) y
# almacenamos su salida estándar al fichero de log indicado por el usuario.
#----------------------------------
# TAREA 4
# Ejecución del programa en C y captura de su salida en el fichero indicado.
#----------------------------------
echo "Lanzando simulación del parking..."
...

# A partir de la salida del programa "parking" en C ($ficheroLog),
# obtenemos los siguientes datos:
#----------------------------------
# TAREA 5
# Utilizar el log del aparcamiento para calcular metricas.
#----------------------------------

# - el número de horas (instantes temporales) en los que el aparcamiento ha estado lleno
horas_lleno=... # número de horas que ha estado a máxima ocupación el aparcamiento
max_horas=...   # número totales de horas que ha estado en funcionamiento
echo "El aparcamiento ha estado al máximo de su capacidad durante $horas_lleno horas de las $max_horas."

# - la espera media para entrar
#       En este subapartado únicamente la definición variable $media es
#       necesaria.
#       El resto de variables son propuestas para guiar la implementación.
#       Es posible que sean necesarias más lineas de código de las presentadas.
horas_entrada=... # array de horas de entrada de cada vehiculo
horas_salida=...  # array de horas de salida de cada vehiculo
diferencias=...   # array de diferencias temporales entre la entrada y la salida
media=...
echo "El tiempo medio de espera para entrar al aparcamiento ha sido de $media horas."

