#ifndef TEXT_BUFFER_H
#define TEXT_BUFFER_H

#include <stdio.h>

#include "akinator_errors.h"

struct text_buffer_t {
    char **containers;
    size_t containers_number;
    size_t storing_strings;
    size_t string_size;
    size_t container_size;
    size_t max_containers_number;
};

akinator_error_t text_buffer_ctor   (text_buffer_t *buffer,
                                     size_t         max_string_size,
                                     size_t         container_size);

akinator_error_t text_buffer_add    (text_buffer_t *buffer,
                                     char         **storage);

akinator_error_t text_buffer_dtor   (text_buffer_t *buffer);

#endif
