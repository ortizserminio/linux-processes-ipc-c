#!/usr/bin/env bash
# Integration tests for parking-workers (exam 2).
# The main check compares the CSV log produced by my solution with the
# reference log provided by the instructor for the same input.
set -u
cd "$(dirname "$0")/../parking-workers" || exit 1
# shellcheck source=tests/lib.sh
. ../tests/lib.sh

echo "== parking-workers"
make -s clean >/dev/null 2>&1
make -s ${CFLAGS:+CFLAGS="$CFLAGS"} || { echo "build failed"; exit 1; }

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# --- matches the instructor's reference output -----------------------------
for run in 1 2 3 4 5; do   # repeated: process scheduling differs per run
    ipc_before=$(ipc_count)
    ./parking data/eventos_profesor.csv "$work/log.csv" > "$work/stdout.txt"
    status=$?
    check "run $run: exits with 0" [ "$status" -eq 0 ]
    check "run $run: log identical to the instructor's reference" \
          diff -q data/log_profesor.csv "$work/log.csv"
    check "run $run: no shared memory or semaphores left behind" \
          [ "$(ipc_count)" -eq "$ipc_before" ]
done

# --- ordering invariant on a larger generated input ------------------------
# 200 events; each car enters through a random gate and later leaves.
awk 'BEGIN {
    srand(7); t = 30000;
    for (i = 1; i <= 100; i++) {
        plate = sprintf("%04dABC", i);
        t += int(rand() * 50) + 1; printf "%d,1,E,%d,%s\n", t, int(rand() * 2), plate;
        t += int(rand() * 50) + 1; printf "%d,0,S,%d,%s\n", t, int(rand() * 2), plate;
    }
}' > "$work/events.csv"
./parking "$work/events.csv" "$work/big.csv" > /dev/null
check "200 events: one log line per event" [ "$(wc -l < "$work/big.csv")" -eq 200 ]
check "200 events: log is in sequence order 1..200" \
      diff -q <(cut -d, -f1 "$work/big.csv") <(seq 1 200)
check "200 events: occupancy ends at 0" [ "$(tail -1 "$work/big.csv" | cut -d, -f7)" -eq 0 ]

# --- argument validation ---------------------------------------------------
expect_exit 1 "rejects missing arguments"         ./parking data/eventos_profesor.csv
expect_exit 1 "rejects a missing events file"     ./parking "$work/nope.csv" "$work/x.csv"
expect_exit 1 "rejects a log name without .csv"   ./parking data/eventos_profesor.csv "$work/log.txt"
expect_exit 1 "rejects a log name that is just .csv" ./parking data/eventos_profesor.csv .csv
expect_exit 1 "rejects a log name with spaces"    ./parking data/eventos_profesor.csv "my log.csv"

make -s clean >/dev/null 2>&1
summary
