#!/usr/bin/env bash
set -euo pipefail

./writer_tick &
WPID=$!

./reader_stress &
./reader_stress &
./reader_stress &
./reader_stress &

wait $WPID
echo "STRESS OK"
