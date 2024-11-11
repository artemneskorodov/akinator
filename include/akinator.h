#ifndef AKINATOR_H
#define AKINATOR_H

#include <stdio.h>

#include "akinator_errors.h"
#include "text_buffer.h"

static const size_t max_nodes_containers_number = 64;

struct akinator_node_t {
    char            *question;
    akinator_node_t *yes;
    akinator_node_t *no;
    akinator_node_t *parent;
};

struct akinator_t {
    akinator_node_t  *root;
    akinator_node_t  *containers[max_nodes_containers_number];
    size_t            container_size;
    size_t            containers_number;
    size_t            used_storage;
    text_buffer_t     new_questions_storage;
    char             *old_questions_storage;
    size_t            questions_storage_position;
    size_t            old_storage_size;
    const char       *database_name;
    FILE             *general_dump;
    size_t            dumps_number;
    akinator_node_t **leafs_array;
    size_t            leafs_array_capacity;
    size_t            leafs_array_size;
};

akinator_error_t akinator_ctor       (akinator_t *akinator,
                                      const char *database_filename);

akinator_error_t akinator_guess      (akinator_t *akinator);

akinator_error_t akinator_dtor       (akinator_t *akinator);

akinator_error_t akinator_definition (akinator_t *akinator);

akinator_error_t akinator_difference (akinator_t *akinator);

#endif
