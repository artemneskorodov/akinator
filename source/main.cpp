#include <locale.h>
#include <windows.h>
#include <stdio.h>
#include <ctype.h>

#include "akinator.h"
#include "akinator_dump.h"
#include "graphics.h"

int main_exit_failure(akinator_t *akinator);

int main(void) {
    akinator_t akinator = {};
    if(akinator_ctor(&akinator, "akinator_database") != AKINATOR_SUCCESS) {
        return main_exit_failure(&akinator);
    }
    while(true) {
        main_menu_cases_t chosen_case = MAIN_MENU_EXIT;
        if(akinator_go_to_main_menu(&chosen_case) != AKINATOR_SUCCESS) {
            return main_exit_failure(&akinator);
        }
        switch(chosen_case) {
            case MAIN_MENU_GO_GUESS: {
                if(akinator_guess(&akinator) != AKINATOR_SUCCESS) {
                    return main_exit_failure(&akinator);
                }
                break;
            }
            case MAIN_MENU_GO_DEFINITION: {
                if(akinator_definition(&akinator) != AKINATOR_SUCCESS) {
                    return main_exit_failure(&akinator);
                }
                break;
            }
            case MAIN_MENU_GO_DIFFERENCE: {
                if(akinator_difference(&akinator) != AKINATOR_SUCCESS) {
                    return main_exit_failure(&akinator);
                }
                break;
            }
            case MAIN_MENU_EXIT: {
                akinator_dtor(&akinator);
                return EXIT_SUCCESS;
            }
            case MAIN_MENU_UNKNOWN_CASE: {
                return main_exit_failure(&akinator);
            }
            default: {
                return main_exit_failure(&akinator);
            }
        }
    }
}

int main_exit_failure(akinator_t *akinator) {
    AKINATOR_DUMP(akinator);
    akinator_dtor(akinator);
    return EXIT_FAILURE;
}
