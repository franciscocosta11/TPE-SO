TP1 - ChompChamps 
Compilar y correr 
correr ./run.sh
Luego, dentro del docker, correr make all
Por ultimo, ./run2.sh
docker run --rm -it -v "$PWD":/work -w /work agodio/itba-so-multi-platform:3.0 /bin/bash

Dentro del contenedor:
make
./master
./view
./player