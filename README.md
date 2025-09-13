TP1 - ChompChamps 
Compilar y correr 
correr ./run.sh (agregar permisos si hace falta con chmod +x)
Luego, dentro del docker, correr make all
Luego correr ./run2.sh para instalar las librerias de ncurses
Por ultimo correr ./run3.sh, para jugar una partida de 2 players
docker run --rm -it -v "$PWD":/work -w /work agodio/itba-so-multi-platform:3.0 /bin/bash

Dentro del contenedor:
make
./master
./view
./player