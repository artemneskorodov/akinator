#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "text_buffer.h"
#include "colors.h"

static const size_t init_text_containers_number = 64;

static akinator_error_t text_buffer_create_new_container(text_buffer_t *buffer);

akinator_error_t text_buffer_ctor(text_buffer_t *buffer, size_t max_string_size, size_t container_size) {
    buffer->string_size           = max_string_size;
    buffer->container_size        = container_size;
    buffer->max_containers_number = init_text_containers_number;
    buffer->storing_strings       = 0;
    buffer->containers            = (char **)calloc(init_text_containers_number, sizeof(char *));
    if(buffer->containers == NULL) {
        color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                     "Error while allocating text buffers storage.\n");
        return AKINATOR_TEXT_CONTAINERS_STORAGE_ERROR;
    }

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

    size_t container = buffer->storing_strings / buffer->container_size;
    size_t index     = buffer->storing_strings % buffer->container_size * buffer->string_size;

    *storage = buffer->containers[container] + index;
    buffer->storing_strings++;
    return AKINATOR_SUCCESS;
}

akinator_error_t text_buffer_dtor(text_buffer_t *buffer) {
    for(size_t element = 0; element < buffer->containers_number; element++) {
        free(buffer->containers[element]);
    }
    free(buffer->containers);
    return AKINATOR_SUCCESS;
}

akinator_error_t text_buffer_create_new_container(text_buffer_t *buffer) {
    if(buffer->containers_number == buffer->max_containers_number) {
        char **new_containers_array = (char **)realloc(buffer->containers, buffer->containers_number * 2);
        if(new_containers_array == NULL) {
            color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                         "Error while reallocating text buffer storage.\n");
            return AKINATOR_TEXT_CONTAINERS_STORAGE_ERROR;
        }
        memset(new_containers_array + buffer->containers_number, 0,
               sizeof(char *) * buffer->containers_number);
        buffer->containers = new_containers_array;
        buffer->max_containers_number *= 2;
    }

    buffer->containers[buffer->containers_number] = (char *)calloc(buffer->container_size,
                                                                   buffer->string_size + 1);
    if(buffer->containers[buffer->containers_number] == NULL) {
        color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                     "Error while allocating text buffer memory.\n");
        return AKINATOR_TEXT_BUFFER_ALLOCATION_ERROR;
    }
    buffer->containers_number++;
    return AKINATOR_SUCCESS;
}
