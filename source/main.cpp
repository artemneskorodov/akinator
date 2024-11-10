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

    printf("%d", akinator_guess(&akinator));
    AKINATOR_DUMP(&akinator);
    akinator_dtor(&akinator);
}
