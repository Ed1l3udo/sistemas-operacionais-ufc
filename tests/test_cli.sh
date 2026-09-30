#!/bin/sh
set -eu

BIN=./build/scheduler${EXEEXT:-}
CONFIG=examples/config.txt
INPUT=examples/processes.txt
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT INT TERM

"$BIN" --config "$CONFIG" --algorithm all --format json --seed 42 < "$INPUT" > "$TMP_DIR/result.json"
grep -q '"algorithm":"fcfs"' "$TMP_DIR/result.json"
grep -q '"algorithm":"priority-rr"' "$TMP_DIR/result.json"
grep -q '"seed":42' "$TMP_DIR/result.json"

"$BIN" --config "$CONFIG" --algorithm rr --format text < "$INPUT" > "$TMP_DIR/result.txt"
grep -q 'Round-Robin' "$TMP_DIR/result.txt"
grep -q 'Trocas de contexto' "$TMP_DIR/result.txt"

"$BIN" --config "$CONFIG" --algorithm srtf --format json --trace < "$INPUT" > "$TMP_DIR/trace.json"
grep -q '"decisions":\[' "$TMP_DIR/trace.json"
grep -q '"reason":"dispatch"' "$TMP_DIR/trace.json"
grep -q '"ready":\[' "$TMP_DIR/trace.json"

if "$BIN" --config "$CONFIG" --algorithm all --format json --trace < "$INPUT" > "$TMP_DIR/out" 2> "$TMP_DIR/error"; then
    echo 'rastreamento de todos os algoritmos foi aceito' >&2
    exit 1
fi
test ! -s "$TMP_DIR/out"
grep -q 'um único algoritmo' "$TMP_DIR/error"

if printf '0 0 1\n' | "$BIN" --config "$CONFIG" --format json > "$TMP_DIR/out" 2> "$TMP_DIR/error"; then
    echo 'entrada inválida foi aceita' >&2
    exit 1
fi
test ! -s "$TMP_DIR/out"
grep -q 'duração > 0' "$TMP_DIR/error"

if printf '0 1 1\n' | "$BIN" --config "$CONFIG" --algorithm nope > "$TMP_DIR/out" 2> "$TMP_DIR/error"; then
    echo 'algoritmo inválido foi aceito' >&2
    exit 1
fi
test ! -s "$TMP_DIR/out"

if "$BIN" --config tests/fixtures/config-missing-aging.txt --format json < "$INPUT" > "$TMP_DIR/out" 2> "$TMP_DIR/error"; then
    echo 'configuração incompleta foi aceita' >&2
    exit 1
fi
test ! -s "$TMP_DIR/out"
grep -q 'quantum e aging' "$TMP_DIR/error"

if "$BIN" --config tests/fixtures/config-zero-quantum.txt --format json < "$INPUT" > "$TMP_DIR/out" 2> "$TMP_DIR/error"; then
    echo 'quantum zero foi aceito' >&2
    exit 1
fi
test ! -s "$TMP_DIR/out"

if "$BIN" --config "$CONFIG" --format json < /dev/null > "$TMP_DIR/out" 2> "$TMP_DIR/error"; then
    echo 'entrada vazia foi aceita' >&2
    exit 1
fi
test ! -s "$TMP_DIR/out"
grep -q 'nenhum processo' "$TMP_DIR/error"

printf 'CLI validada.\n'
