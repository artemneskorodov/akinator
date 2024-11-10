#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "akinator_utils.h"

static const size_t max_system_command_length = 256;

bool is_leaf(akinator_node_t *node) {
    if(node->no == NULL) {
        return true;
    }
    else {
        return false;
    }
}

akinator_error_t akinator_run_system_command(const char *format, ...) {
    char command[max_system_command_length] = {};

    va_list args;
    va_start(args, format);
    if(vsprintf(command, format, args) < 0) {
        return AKINATOR_SYSTEM_COMMAND_ERROR;
    }
    va_end(args);
    system(command);

    return AKINATOR_SUCCESS;
}

size_t file_size(FILE *file) {
    int position = ftell(file);
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, position, SEEK_SET);
    return size;
}
