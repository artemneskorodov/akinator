#ifndef AKINATOR_UTILS_H
#define AKINATOR_UTILS_H

#include "akinator_errors.h"
#include "akinator.h"

bool             is_leaf                     (akinator_node_t *node);

akinator_error_t akinator_run_system_command (const char      *format, ...);

size_t           file_size                   (FILE            *file);

#endif
