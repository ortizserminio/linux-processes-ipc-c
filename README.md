# Linux processes and IPC in C: parking simulations

**English** · [Español](README.es.md)

[![CI](https://github.com/ortizserminio/linux-processes-ipc-c/actions/workflows/ci.yml/badge.svg)](https://github.com/ortizserminio/linux-processes-ipc-c/actions/workflows/ci.yml)

Two multi-process programs for Linux, written for the practical exams of *Sistemas Operativos* (Universidad Miguel Hernández, 2025/26). Both simulate a car park using only POSIX and System V primitives: `fork`, `wait`, pipes, shared memory (`shmget` / `shmat`) and semaphores (`semget` / `semop`). A Bash launcher and integration tests run in CI on Ubuntu.

| Folder | Processes | What it shows |
|---|---|---|
| [`parking-two-gates/`](parking-two-gates) | 1 parent (clock) + 2 entry gates + 1 exit | A tick-driven simulation: the parent broadcasts the time through pipes; children update shared state under a mutex |
| [`parking-workers/`](parking-workers) | 1 parent (dispatcher) + 4 workers | **Ordering across processes**: events reach different workers in parallel, but must be applied and logged in global sequence order |

## Exam 1: two entry gates and one exit (`parking-two-gates`)

```
                 pipe: t, n, VEHICULO...   ┌──────────────┐
            ┌─────────────────────────────▶│ entry gate 0 │──┐
┌────────┐  │  pipe: t, n, VEHICULO...     ├──────────────┤  │   sWait / sSignal
│ parent │──┼─────────────────────────────▶│ entry gate 1 │──┼──▶ shared memory: struct ESTADO
│ (clock)│  │  pipe: t                     ├──────────────┤  │    {capacity, occupancy, cars[]}
└────────┘  └─────────────────────────────▶│     exit     │──┘
                                           └──────────────┘
```

1. The parent validates the arguments and loads each gate's arrivals (`plate  arrival  departure`). It creates one shared-memory segment for the parking state, one semaphore used as a **mutex**, and one pipe per child, then forks the children.
2. **Simulated clock:** for every tick `t`, the parent writes `t` to every pipe. To each entry gate it then writes how many cars arrive at `t`, followed by those `struct VEHICULO` records. After the last tick it sends `-1`.
3. **Entry gates** queue the cars they receive. Inside the mutex, they admit cars while there is free space and append them to the shared array.
4. **The exit process**, inside the mutex, removes every car whose departure time has come and compacts the array.
5. The parent closes its pipe ends, detaches and removes the shared memory, destroys the semaphore and `wait`s for every child.

`lanzar_parking.sh` splits a combined input file by gate, runs the program, captures its output into a log, and computes from the log how many ticks the car park was full and the mean waiting time to enter.

## Exam 2: four workers with global ordering (`parking-workers`)

The parent reads a CSV of events (`second, priority, E|S, gate, plate`) and numbers them `seq = 1, 2, 3…`. It sends each event to one of **4 worker processes** (entry/exit × gate 0/1), each over its own pipe. Workers run in parallel, but the log must list events in exact `seq` order. This is solved with shared memory and semaphores:

* `nextSeq`, in shared memory, is the next event allowed to proceed.
* A worker takes the mutex and compares its event's `seq` with `*nextSeq`. If it is not its turn, it sets its `esperando[i]` flag (still holding the mutex), releases the mutex and sleeps on **its own** semaphore `sTurno[i]`.
* The worker whose turn it is updates the parking state, writes its CSV log line, increments `nextSeq` and, before releasing the mutex, **wakes every waiting worker**: it clears each set flag and signals that worker's semaphore. Each woken worker re-checks its turn.

The exam statement prescribed a different wake-up, signalling one shared semaphore `NUM_WORKERS − 1` times. That version can deadlock (see below), so it was replaced after the exam.

The result must match the instructor's reference log line for line, and the tests check exactly that.

## Build and test (Linux)

```bash
cd parking-two-gates && make && ./parking <gate0.txt> <gate1.txt> <capacity>
cd parking-workers   && make && ./parking data/eventos_profesor.csv out.csv

bash tests/test_parking_two_gates.sh
bash tests/test_parking_workers.sh
```

The integration tests ([`tests/`](tests)) check:

* **Exam 1**, for capacities 4 and 10:
  - every car enters and leaves exactly once;
  - occupancy never exceeds capacity;
  - the car park ends empty;
  - **no shared-memory segment or semaphore is leaked** (`ipcs` before and after);
  - invalid arguments are rejected (wrong count, non-numeric or zero capacity, missing file, departure before arrival, unsorted arrivals);
  - the launcher works end to end and reports the right full-capacity hours.
* **Exam 2**:
  - five runs whose log must be **identical to the instructor's reference** (scheduling differs between runs);
  - 200 generated events whose log must come out in sequence order 1..200;
  - a **stress test** of 30 runs, each under `timeout`, so a deadlock fails the build instead of hanging it;
  - argument validation.

**CI** ([`.github/workflows/ci.yml`](.github/workflows/ci.yml)) runs both suites twice on Ubuntu, once with a plain `-O2` build and once with **AddressSanitizer + UndefinedBehaviorSanitizer**, and runs **ShellCheck** on the scripts.

## Bugs found and fixed after the exam

Each fix is in its own commit:

* **Undefined behaviour in C:** the entry queue and the shared array of parked cars were compacted with `memcpy` from element `i+1` onto `i`. Source and destination overlap, which is undefined for `memcpy` (AddressSanitizer reports `memcpy-param-overlap`). Replaced with `memmove`.
* **Deadlock in exam 2 (found by CI):** the turn-taking protocol in the exam statement "broadcasts" by signalling one shared semaphore `NUM_WORKERS − 1` times. A worker that is not next can wake up, re-check, block again and **steal a second signal**, leaving the worker whose turn it is asleep forever. It depends on scheduling: it never showed up in class or in the sanitizer build, but it hung the `-O2` build on GitHub's Linux runners. Fixed with an emulated condition variable: one semaphore per worker plus an `esperando[]` flag that is set and read under the mutex, so a wake-up can be neither lost nor stolen. A stress test now runs 200 events 30 times under `timeout`.
* **Wrong metric in Bash:** "hours at full capacity" matched `4/4` anywhere in the log, including the per-entry `Ocupacion actualizada: 4/4` lines. It reported **6** hours instead of **2** (out of 13 ticks) on the exam example.
* **Launcher robustness:** it did not create its output folder, divided by zero when no car entered, and wrote blank input lines into gate 0's file (in Bash, `[[ "" -eq 0 ]]` is true). The last one was found by CI.

## Code provenance

These were exams with an instructor-provided skeleton, so part of each file is not mine:

| | Written by the instructor | Written by me |
|---|---|---|
| `parking-two-gates/parking.c`, `lanzar_parking.sh` | Skeleton, comments and helper functions | Everything inside the `TAREA n` blocks |
| `parking-workers/parking.c` | Headers, structs, helpers marked `NO TOCAR`, comments | Everything between `BEGIN-TODO-n` and `END-TODO`, plus the deadlock fix (marked *Post-exam fix*) |
| `sem.c`, `sem.h`, `makefile`, `data/` | All (semaphore library, build file, example and reference data) | — |
| `tests/`, `.github/`, this README | — | All |

The first commit contains only the instructor's files. For exam 1, `git diff 2080198 23c99a0` shows exactly the lines I wrote. The [LICENSE](LICENSE) (MIT) covers only my code.

The tests, CI, bug analysis and this documentation were prepared with the help of an AI assistant. The exam solutions are my own work.
