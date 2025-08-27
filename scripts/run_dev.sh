#!/usr/bin/env bash
set -euo pipefail

# Compila todo y ejecuta la vista (por ahora sin jugadores).
# Cuando master esté listo, podés hacer:
#   ./bin/master & sleep 0.2; ./bin/view
# Por hoy, dejamos solo build + vista (que usa dummy si no hay shm).

make
echo "== corriendo vista =="
./bin/view
