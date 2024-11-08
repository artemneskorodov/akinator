#include <stdio.h>
#include <stdlib.h>

#include "text_buffer.h"
#include "colors.h"

static akinator_error_t text_buffer_create_new_container(text_buffer_t *buffer);

akinator_error_t text_buffer_ctor(text_buffer_t *buffer, size_t max_string_size, size_t container_size) {
    buffer->string_size = max_string_size;
    buffer->container_size = container_size;
    buffer->storing_strings = 0;
    akinator_error_t error_code = AKINATOR_SUCCESS;
    if((error_code = text_buffer_create_new_container(buffer)) != AKINATOR_SUCCESS) {
        return error_code;
    }

    return AKINATOR_SUCCESS;
}

akinator_error_t text_buffer_add (text_buffer_t *buffer, char **storage) {
    akinator_error_t error_code = AKINATOR_SUCCESS;
    if(buffer->container_size * buffer->containers_number == buffer->storing_strings) {
        if((error_code = text_buffer_create_new_container(buffer)) != AKINATOR_SUCCESS) {
            return error_code;
        }
    }

    *storage = buffer->containers[buffer->storing_strings / buffer->container_size] + buffer->storing_strings % buffer->container_size * buffer->string_size;
    buffer->storing_strings++;
    return AKINATOR_SUCCESS;
}

akinator_error_t text_buffer_dtor(text_buffer_t *buffer) {
    for(size_t element = 0; element < buffer->containers_number; element++) {
        free(buffer->containers[element]);
    }
    return AKINATOR_SUCCESS;
}

akinator_error_t text_buffer_create_new_container(text_buffer_t *buffer) {
    buffer->containers[buffer->containers_number] = (char *)calloc(buffer->container_size, buffer->string_size + 1);
    if(buffer->containers[buffer->containers_number] == NULL) {
        color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                     "Error while allocating text buffer memory.\n");
        return AKINATOR_TEXT_BUFFER_ALLOCATION_ERROR;
    }
    buffer->containers_number++;
    return AKINATOR_SUCCESS;
}
