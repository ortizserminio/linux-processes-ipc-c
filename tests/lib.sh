#!/usr/bin/env bash
# Small helpers shared by the integration tests.

PASSED=0
FAILED=0

pass() { PASSED=$((PASSED + 1)); echo "  ok   - $1"; }
fail() { FAILED=$((FAILED + 1)); echo "  FAIL - $1"; }

# check "<description>" <command...>: passes if the command succeeds.
check() {
    local desc="$1"
    shift
    if "$@"; then pass "$desc"; else fail "$desc"; fi
}

# expect_exit <code> "<description>" <command...>
expect_exit() {
    local expected="$1" desc="$2"
    shift 2
    "$@" >/dev/null 2>&1
    local got=$?
    if [ "$got" -eq "$expected" ]; then
        pass "$desc (exit $got)"
    else
        fail "$desc (expected exit $expected, got $got)"
    fi
}

# Number of System V shared-memory segments and semaphore sets owned by us.
ipc_count() {
    echo $(( $(ipcs -m | grep -c "$(whoami)") + $(ipcs -s | grep -c "$(whoami)") ))
}

summary() {
    echo "== $PASSED passed, $FAILED failed"
    [ "$FAILED" -eq 0 ]
}
