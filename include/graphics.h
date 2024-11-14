#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "akinator_errors.h"
#include "TX/TXLib.h"

enum main_menu_cases_t {
    MAIN_MENU_GO_GUESS,
    MAIN_MENU_GO_DIFFERENCE,
    MAIN_MENU_GO_DEFINITION
    MAIN_MENU_EXIT
};

akinator_error_t akinator_graphics_init(void);
akinator_error_t akinator_go_to_main_menu(main_menu_cases_t *output);

#endif
