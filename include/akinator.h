#ifndef AKINATOR_H
#define AKINATOR_H

#include <stdio.h>

#include "akinator_errors.h"
#include "text_buffer.h"

static const size_t max_nodes_containers_number = 64;

struct akinator_node_t {
    char *question;
    akinator_node_t *yes;
    akinator_node_t *no;
};

struct akinator_t {
    akinator_node_t *root;
    akinator_node_t *containers[max_nodes_containers_number];
    size_t container_size;
    size_t containers_number;
    size_t used_storage;
    text_buffer_t questions_storage;
    const char *database_name;

    FILE *general_dump;
    size_t dumps_number;
};

akinator_error_t akinator_ctor(akinator_t *akinator, const char *database_filename);
akinator_error_t akinator_guess(akinator_t *akinator);
akinator_error_t akinator_dtor(akinator_t *akinator);
akinator_error_t akinator_dump(akinator_t *akinator);

//Get-Content dump.dot | Set-Content -Encoding utf8 test_utf.txt

#endif
