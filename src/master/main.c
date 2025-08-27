#include <stdio.h>
#include <stdlib.h>
#include "shared_data.h"
#include "master_logic.h"

void say_hello(const char *who);

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    say_hello("master");
    return 0;
}