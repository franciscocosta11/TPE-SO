TP1 - ChompChamps (stub etapa 0)
Compilar y correr (SIEMPRE en la imagen de la cátedra)
docker pull agodio/itba-so-multi-platform:3.0
docker run --rm -it -v "$PWD":/work -w /work agodio/itba-so-multi-platform:3.0 /bin/bash

Dentro del contenedor:
make
./master
./view
./player