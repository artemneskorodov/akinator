#include <locale.h>
#include <windows.h>
#include <stdio.h>
#include <ctype.h>

#include "akinator.h"
#include "akinator_dump.h"

int main(void) {

    akinator_t akinator = {};
    printf("%d\n", akinator_ctor(&akinator, "akinator_database"));

    AKINATOR_DUMP(&akinator);

    printf("%d\n", akinator_guess(&akinator));
    printf("%d\n", akinator_definition(&akinator));
    printf("%d\n", akinator_difference(&akinator));
    AKINATOR_DUMP(&akinator);
    akinator_dtor(&akinator);

    printf("exit success");
    return EXIT_SUCCESS;
}
