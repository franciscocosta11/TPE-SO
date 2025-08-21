#include <stdio.h>
#include <stdlib.h>

void say_hello(const char *who);

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    say_hello("player");
    return 0;
}
