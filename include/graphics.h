#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "akinator_errors.h"
#include "akinator.h"

enum main_menu_cases_t : size_t {
    MAIN_MENU_UNKNOWN_CASE  = 228,
    MAIN_MENU_GO_GUESS      = 0,
    MAIN_MENU_GO_DEFINITION = 1,
    MAIN_MENU_GO_DIFFERENCE = 2,
    MAIN_MENU_EXIT          = 3
};

akinator_error_t akinator_graphics_init     (void);
akinator_error_t akinator_go_to_main_menu   (main_menu_cases_t *output);
akinator_error_t akinator_ui_set_guess      (void);
akinator_error_t akinator_ui_set_definition (void);
akinator_error_t akinator_ui_set_difference (void);
akinator_error_t akinator_ui_write_message  (const char        *message);
akinator_error_t akinator_get_answer_yes_no (akinator_answer_t *answer);
akinator_error_t akinator_get_text_answer   (char              *output);
akinator_error_t akinator_graphics_dtor     (void);

#endif
