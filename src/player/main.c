#include <unistd.h>   // write, usleep
#include <stdint.h>   // uint8_t
#include <stdio.h>    // setvbuf

int main(void) {
    // Desactivar buffering de stdout por las dudas (aunque usamos write)
    setvbuf(stdout, NULL, _IONBF, 0);

    for (int i = 0; i < 20; ++i) {
        uint8_t move = (uint8_t)(i % 8);   // 0..7
        ssize_t n = write(1, &move, 1);    // 1 byte por “turno”
        (void)n;                           // ignoramos errores por ahora
        sleep(1);                // 100 ms para que se vea la ronda
    }
    return 0; // EOF para el máster
}