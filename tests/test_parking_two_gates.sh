#!/usr/bin/env bash
# Integration tests for parking-two-gates (exam 1).
# Builds the program, runs it on the example input and checks the
# simulation invariants, argument validation and IPC cleanup.
set -u
cd "$(dirname "$0")/../parking-two-gates" || exit 1
# shellcheck source=tests/lib.sh
. ../tests/lib.sh

echo "== parking-two-gates"
make -s clean >/dev/null 2>&1
make -s ${CFLAGS:+CFLAGS="$CFLAGS"} || { echo "build failed"; exit 1; }

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# Split the example (plate, gate, arrival, departure) into one file per gate.
awk -F '\t' 'NF==4 && $2==0 {print $1"\t"$3"\t"$4}' data/entrada_vehiculos.txt > "$work/gate0.txt"
awk -F '\t' 'NF==4 && $2==1 {print $1"\t"$3"\t"$4}' data/entrada_vehiculos.txt > "$work/gate1.txt"
cars=$(grep -c . "$work/gate0.txt" "$work/gate1.txt" | awk -F: '{s+=$2} END {print s}')

# --- normal run ------------------------------------------------------------
for capacity in 4 10; do
    ipc_before=$(ipc_count)
    ./parking "$work/gate0.txt" "$work/gate1.txt" "$capacity" > "$work/log.txt"
    status=$?
    check "capacity $capacity: exits with 0" [ "$status" -eq 0 ]

    entered=$(grep -c "ha entrado" "$work/log.txt")
    left=$(grep -c "ha salido" "$work/log.txt")
    check "capacity $capacity: all $cars cars enter ($entered)" [ "$entered" -eq "$cars" ]
    check "capacity $capacity: all $cars cars leave ($left)" [ "$left" -eq "$cars" ]

    # Every plate enters exactly once and leaves exactly once.
    dup=$(grep -oE '[0-9]{4}[A-Z]{3} ha (entrado|salido)' "$work/log.txt" | sort | uniq -d | wc -l)
    check "capacity $capacity: no car enters or leaves twice" [ "$dup" -eq 0 ]

    # Occupancy is never above capacity and never negative.
    max_occ=$(grep -oE '(ocupacion=|Ocupacion actualizada: )[0-9]+' "$work/log.txt" \
              | grep -oE '[0-9]+$' | sort -n | tail -1)
    check "capacity $capacity: occupancy never exceeds capacity (max $max_occ)" \
          [ "$max_occ" -le "$capacity" ]

    last=$(grep -E '^\[T=[0-9]+\]\s+ocupacion=' "$work/log.txt" | tail -1)
    check "capacity $capacity: parking ends empty ($last)" \
          grep -q "ocupacion=0/$capacity" <<< "$last"

    check "capacity $capacity: no shared memory or semaphores left behind" \
          [ "$(ipc_count)" -eq "$ipc_before" ]
done

# With capacity 4 the parking is full at T=2 (4 cars inside), as in the exam.
./parking "$work/gate0.txt" "$work/gate1.txt" 4 > "$work/log4.txt"
check "capacity 4: parking reaches 4/4" grep -q "ocupacion=4/4" "$work/log4.txt"

# --- argument validation ---------------------------------------------------
expect_exit 1 "rejects missing arguments"   ./parking "$work/gate0.txt" "$work/gate1.txt"
expect_exit 1 "rejects non-numeric capacity" ./parking "$work/gate0.txt" "$work/gate1.txt" abc
expect_exit 1 "rejects capacity 0"           ./parking "$work/gate0.txt" "$work/gate1.txt" 0
expect_exit 1 "rejects negative capacity"    ./parking "$work/gate0.txt" "$work/gate1.txt" -3
expect_exit 1 "rejects a missing input file" ./parking "$work/nope.txt" "$work/gate1.txt" 4
printf 'AAAA111\t5\t3\n' > "$work/bad.txt"   # leaves before it arrives
expect_exit 1 "rejects departure before arrival" ./parking "$work/bad.txt" "$work/gate1.txt" 4
printf 'AAAA111\t5\t9\nBBBB222\t2\t9\n' > "$work/unsorted.txt"
expect_exit 1 "rejects arrivals out of order" ./parking "$work/unsorted.txt" "$work/gate1.txt" 4

# --- launcher script -------------------------------------------------------
cp parking lanzar_parking.sh data/entrada_vehiculos.txt "$work/"
(
    cd "$work" || exit 1   # no accesos/ folder: the script must create it
    ./lanzar_parking.sh entrada_vehiculos.txt 4 out.log > script.out 2>&1
) ; status=$?
check "launcher: runs successfully" [ "$status" -eq 0 ]
check "launcher: reports hours at full capacity" grep -q "capacidad durante" "$work/script.out"
check "launcher: reports the mean waiting time" grep -q "tiempo medio de espera" "$work/script.out"
# Full hours must count only the per-tick status lines ("ocupacion=4/4"),
# not the "Ocupacion actualizada: 4/4" lines printed on each entry.
full_ticks=$(grep -c "ocupacion=4/4" "$work/out.log")
check "launcher: full-capacity hours counted per tick ($full_ticks)" \
      grep -q "capacidad durante $full_ticks horas" "$work/script.out"
check "launcher: splits the input per gate" \
      [ "$(wc -l < "$work/accesos/entrada_0.txt")" -eq 4 ]
expect_exit 1 "launcher: rejects wrong argument count" "$work/lanzar_parking.sh" a b
expect_exit 1 "launcher: rejects missing input file"   "$work/lanzar_parking.sh" /nope 4 x.log
expect_exit 1 "launcher: rejects capacity 0"           "$work/lanzar_parking.sh" "$work/entrada_vehiculos.txt" 0 x.log

make -s clean >/dev/null 2>&1
summary
