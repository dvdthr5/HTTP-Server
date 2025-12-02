#!/bin/sh
# GitLab CI entrypoint. Compiles finished modules and reports skipped ones
# so the pipeline gives fast feedback even while portions are still WIP.

set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CONFIG_FILE="$SCRIPT_DIR/tests/test-config.txt"
BUILD_DIR="$SCRIPT_DIR/obj/ci"
INCLUDE_FLAG="-I$SCRIPT_DIR/src"
CFLAGS="-Wall -Wextra -Werror -pedantic -std=c11 $INCLUDE_FLAG"
TEST_CFLAGS="-Wall -Wextra -pedantic -std=c11 $INCLUDE_FLAG"
TEST_LDFLAGS="-lpthread"

mkdir -p "$BUILD_DIR"
trap 'rm -rf "$BUILD_DIR"' EXIT

TMP_TEST_DIR="$SCRIPT_DIR/tests/tmp"
mkdir -p "$TMP_TEST_DIR"
export TEST_TMPDIR="$TMP_TEST_DIR"

build_object_if_needed() {
    name="$1"
    src="$SCRIPT_DIR/src/${name}.c"
    tmp="$BUILD_DIR/${name}.c"
    obj="$BUILD_DIR/${name}.o"

    if [ ! -f "$obj" ]; then
        cp "$src" "$tmp"
        gcc $CFLAGS -c "$tmp" -o "$obj"
    fi
}

### HTTP TESTS ###
run_http_tests() {
    HTTP_OBJ="$BUILD_DIR/http.o"
    if [ ! -f "$HTTP_OBJ" ]; then
        echo "[WARN] http.o missing after build; recompiling for tests."
        gcc $CFLAGS -c "$SCRIPT_DIR/src/http.c" -o "$HTTP_OBJ"
    fi

    echo "[TEST] Building http parser tests"
    gcc $TEST_CFLAGS "$SCRIPT_DIR/tests/test_http.c" "$HTTP_OBJ" -o "$BUILD_DIR/test_http"
    echo "[RUN] Running http parser tests"
    "$BUILD_DIR/test_http"
}

### DISPATCHER TESTS ###
run_dispatcher_tests() {
    DISP_OBJ="$BUILD_DIR/dispatcher.o"
    FILE_OBJ="$BUILD_DIR/file.o"
    HTTP_OBJ="$BUILD_DIR/http.o"

    for dep in "$DISP_OBJ" "$FILE_OBJ" "$HTTP_OBJ"; do
        if [ ! -f "$dep" ]; then
            echo "[ERROR] Missing dependency object: $dep" >&2
            exit 1
        fi
    done

    mkdir -p "$SCRIPT_DIR/tests/tmp"

    echo "[TEST] Building dispatcher tests"
    gcc $TEST_CFLAGS \
        "$SCRIPT_DIR/tests/test_dispatcher.c" \
        "$DISP_OBJ" \
        "$FILE_OBJ" \
        "$HTTP_OBJ" \
        -o "$BUILD_DIR/test_dispatcher"

    echo "[RUN] Running dispatcher tests"
    TEST_TMPDIR="$SCRIPT_DIR/tests/tmp" "$BUILD_DIR/test_dispatcher"
}

### FILE TESTS ###
run_file_tests() {
    FILE_OBJ="$BUILD_DIR/file.o"
    if [ ! -f "$FILE_OBJ" ]; then
        echo "[WARN] file.o missing; recompiling."
        gcc $CFLAGS -c "$SCRIPT_DIR/src/file.c" -o "$FILE_OBJ"
    fi

    echo "[TEST] Building file tests"
    gcc $TEST_CFLAGS "$SCRIPT_DIR/tests/test_file.c" "$FILE_OBJ" -o "$BUILD_DIR/test_file"
    echo "[RUN] Running file tests"
    "$BUILD_DIR/test_file"
}

run_log_tests() {
    LOG_OBJ="$BUILD_DIR/log.o"
    if [ ! -f "$LOG_OBJ" ]; then
        echo "[WARN] log.o missing; recompiling."
        gcc $CFLAGS -c "$SCRIPT_DIR/src/log.c" -o "$LOG_OBJ"
    fi

    echo "[TEST] Building log tests"
    gcc $TEST_CFLAGS "$SCRIPT_DIR/tests/test_log.c" "$LOG_OBJ" -o "$BUILD_DIR/test_log" $TEST_LDFLAGS
    echo "[RUN] Running log tests"
    "$BUILD_DIR/test_log"
}

run_server_tests() {
    SERVER_OBJ="$BUILD_DIR/server.o"
    if [ ! -f "$SERVER_OBJ" ]; then
        echo "[WARN] server.o missing; recompiling."
        gcc $CFLAGS -c "$SCRIPT_DIR/src/server.c" -o "$SERVER_OBJ"
    fi

    echo "[TEST] Building server tests"
    gcc $TEST_CFLAGS "$SCRIPT_DIR/tests/test_server.c" "$SERVER_OBJ" -o "$BUILD_DIR/test_server" $TEST_LDFLAGS
    echo "[RUN] Running server tests"
    "$BUILD_DIR/test_server"
}

run_connection_tests() {
    for dep in connection http dispatcher file log; do
        build_object_if_needed "$dep"
    done

    echo "[TEST] Building connection tests"
    gcc $TEST_CFLAGS \
        "$SCRIPT_DIR/tests/test_connection.c" \
        "$BUILD_DIR/connection.o" \
        "$BUILD_DIR/http.o" \
        "$BUILD_DIR/dispatcher.o" \
        "$BUILD_DIR/file.o" \
        "$BUILD_DIR/log.o" \
        -o "$BUILD_DIR/test_connection"

    echo "[RUN] Running connection tests"
    "$BUILD_DIR/test_connection"
}

run_parallel_tests() {
    for dep in parallel connection http dispatcher file log; do
        build_object_if_needed "$dep"
    done

    echo "[TEST] Building parallel tests"
    gcc $TEST_CFLAGS \
        "$SCRIPT_DIR/tests/test_parallel.c" \
        "$BUILD_DIR/parallel.o" \
        "$BUILD_DIR/connection.o" \
        "$BUILD_DIR/http.o" \
        "$BUILD_DIR/dispatcher.o" \
        "$BUILD_DIR/file.o" \
        "$BUILD_DIR/log.o" \
        -o "$BUILD_DIR/test_parallel" \
        $TEST_LDFLAGS

    echo "[RUN] Running parallel tests"
    "$BUILD_DIR/test_parallel"
}

run_main_tests() {
    echo "[TEST] Building main tests"
    gcc $TEST_CFLAGS -DUNIT_TEST \
        "$SCRIPT_DIR/tests/test_main.c" \
        "$SCRIPT_DIR/src/main.c" \
        -o "$BUILD_DIR/test_main"
    echo "[RUN] Running main tests"
    "$BUILD_DIR/test_main"
}

RUN_DISPATCH_AFTER_FILE=0
tests_ran=0

### MAIN LOOP ###
while IFS='=' read -r module status || [ -n "$module" ]; do
    [ -z "$module" ] && continue
    module_upper=$(printf '%s' "$module" | tr '[:lower:]' '[:upper:]')
    status_lower=$(printf '%s' "$status" | tr '[:upper:]' '[:lower:]')

    case "$module_upper" in
        MAIN) src_name="main" ;;
        SERVER) src_name="server" ;;
        HTTP) src_name="http" ;;
        CONNECTION) src_name="connection" ;;
        DISPATCHER) src_name="dispatcher" ;;
        FILE) src_name="file" ;;
        LOG) src_name="log" ;;
        PARALLEL) src_name="parallel" ;;
        *)
            echo "Unknown module '$module_upper' in $CONFIG_FILE" >&2
            exit 1
            ;;
    esac

    src_file="$SCRIPT_DIR/src/${src_name}.c"
    if [ ! -f "$src_file" ]; then
        echo "Source file missing for module $module_upper: $src_file" >&2
        exit 1
    fi

    if [ "$status_lower" != "done" ]; then
        echo "[SKIP] $module_upper is $status_lower; not compiling yet."
        continue
    fi

    tmp_src="$BUILD_DIR/${src_name}.c"
    cp "$src_file" "$tmp_src"

    obj_file="$BUILD_DIR/${src_name}.o"
    echo "[TEST] Compiling $module_upper -> $(basename "$obj_file")"
    gcc $CFLAGS -c "$tmp_src" -o "$obj_file"
    tests_ran=$((tests_ran + 1))

    case "$module_upper" in
        HTTP)
            run_http_tests
            ;;
        DISPATCHER)
            RUN_DISPATCH_AFTER_FILE=1
            ;;
        FILE)
            run_file_tests
            if [ "$RUN_DISPATCH_AFTER_FILE" -eq 1 ]; then
                run_dispatcher_tests
            fi
            ;;
        LOG)
            run_log_tests
            ;;
        SERVER)
            run_server_tests
            ;;
        CONNECTION)
            run_connection_tests
            ;;
        MAIN)
            run_main_tests
            ;;
        PARALLEL)
            run_parallel_tests
            ;;
    esac

done < "$CONFIG_FILE"

if [ "$tests_ran" -eq 0 ]; then
    echo "No modules marked DONE; CI confirms scaffolding only."
fi
