#include <stdio.h>
#include "./helpers/vector.h"
#include "compiler.h"
int main()
{
    int res = compile_file("test.s", "test.out", 0);
    if (res == COMPILER_FILE_COMPILED_OK)
        printf("Compiled successfully.\n");
    else if (res == COMPILER_FAILED_WITH_ERRORS)
        printf("Compile failed.\n");
    else
        printf("Unknown error\n");

    return 0;
}
