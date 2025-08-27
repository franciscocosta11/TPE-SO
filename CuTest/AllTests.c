// Runner de tests del proyecto
#include <stdio.h>
#include "CuTest.h"

// Suite propia definida en tests/test_state.c
CuSuite* StateGetSuite(void);

int RunAllTests(void) {
    CuString *output = CuStringNew();
    CuSuite  *suite  = CuSuiteNew();

    // Agregar suites del proyecto
    CuSuiteAddSuite(suite, StateGetSuite());

    // Ejecutar
    CuSuiteRun(suite);
    CuSuiteSummary(suite, output);
    CuSuiteDetails(suite, output);
    printf("%s", output->buffer);
    return suite->failCount;
}

int main(void) { return RunAllTests(); }
