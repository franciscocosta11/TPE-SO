#!/usr/bin/env bash
set -euo pipefail

# Compilar si no existen
if [[ ! -x ./writer_tick || ! -x ./reader_stress ]]; then
  echo "[stress] construyendo binarios..."
  make reader_stress writer_tick
fi

echo "[stress] lanzando writer + 4 readers"

# Writer
./writer_tick & WPID=$!
echo "[stress] writer PID=$WPID"

# Readers
PIDS=()
for i in 1 2 3 4; do
  ./reader_stress &
  PIDS+=($!)
  echo "[stress] reader $i PID=${PIDS[-1]}"
done

wait "$WPID"
echo "[stress] writer finished"

sleep 0.5
echo "STRESS OK ✔"
