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
    if [ "$#" -ne 3 ]; then
        echo "ERROR: número incorrecto de parámetros."
        Sintaxis
        exit 1
    fi

    # Comprobamos que el primer parámetro es un fichero existente.
    if [ ! -f "$1" ]; then
        echo "ERROR: el parámetro <ficheroCoches> debe ser un fichero existente."
        Sintaxis
        exit 1
    fi

    # Comprobamos que el segundo parámetro es numérico.
    if ! Number "$2"; then
        echo "ERROR: el parámetro <capacidadTotal> ($2) debe ser un número entero."
        Sintaxis
        exit 1
    fi

    # Comprobamos rangos lógicos: el argumento numérico es estrictamente
    # mayor que 0.
    if [ "$2" -le 0 ]; then
        echo "ERROR: <capacidadTotal> debe ser mayor que 0."
        Sintaxis
        exit 1
    fi
}

# Programa principal

#----------------------------------
# TAREA 2
# Comprobación de argumentos y asignacion de variables.
#----------------------------------
# Comprobamos los argumentos con la función previamente implementada
CheckArguments "$@"

# Asignamos los argumentos de entrada a variables mas cómodas
ficheroCoches="$1"
capacidad="$2"
ficheroLog="$3"

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
# Version con awk
# awk -F '\t' '$2==1 { printf "%s\t%d\t%d\n", $1, $3, $4 }' "$ficheroCoches" > "$archivoEntrada0"
# awk -F '\t' '$2==2 { printf "%s\t%d\t%d\n", $1, $3, $4 }' "$ficheroCoches" > "$archivoEntrada1"
# Leyendo el archivo linea a linea
> $archivoEntrada0
> $archivoEntrada1
while IFS=$'\t' read -r col1 col2 col3 col4; do
    if [[ "$col2" -eq 0 ]]; then
        echo -e "$col1\t$col3\t$col4" >> $archivoEntrada0
    fi
    if [[ "$col2" -eq 1 ]]; then
        echo -e "$col1\t$col3\t$col4" >> $archivoEntrada1
    fi
done < "$ficheroCoches"

# Lanzamos el programa en C (parking) y
# almacenamos su salida estándar al fichero de log indicado por el usuario.
#----------------------------------
# TAREA 4
# Ejecución del programa en C y captura de su salida en el fichero indicado.
#----------------------------------
echo "Lanzando simulación del parking..."
./parking "$archivoEntrada0" "$archivoEntrada1" "$capacidad" > "$ficheroLog"

# A partir de la salida del programa "parking" en C ($ficheroLog),
# obtenemos los siguientes datos:
#----------------------------------
# TAREA 5
# Utilizar el log del aparcamiento para calcular metricas.
#----------------------------------

# - el número de horas (instantes temporales) en los que el aparcamiento ha estado lleno
horas_lleno=$(grep "$capacidad/$capacidad" $ficheroLog | wc -l)
max_horas=$(grep ocupacion $ficheroLog | wc -l)
echo "El aparcamiento ha estado al máximo de su capacidad durante $horas_lleno horas de las $max_horas."

# - la espera media para entrar
#       En este subapartado únicamente la definición variable $media es
#       necesaria.
#       El resto de variables son propuestas para guiar la implementación.
#       Es posible que sean necesarias más lineas de código de las presentadas.
horas_entrada=($(grep "entrado" $ficheroLog | cut -d ] -f1 | cut -d = -f2))
horas_salida=($(grep "entrado" $ficheroLog | cut -d "=" -f3 | cut -d ' ' -f1))
diferencias=()
for ((i=0; i<${#horas_entrada[@]}; i++)); do
    diferencias[$i]=$((horas_entrada[i] - horas_salida[i]))
done
suma=0
for val in "${diferencias[@]}"; do
    suma=$((suma+val))
done
media=$(echo "scale=2; $suma / ${#diferencias[@]}" | bc)
echo "El tiempo medio de espera para entrar al aparcamiento ha sido de $media horas."

