#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "akinator.h"
#include "text_buffer.h"
#include "colors.h"
#include "akinator_utils.h"
#include "akinator_dump.h"

enum akinator_answer_t {
    AKINATOR_ANSWER_YES     = 0,
    AKINATOR_ANSWER_NO      = 1,
    AKINATOR_ANSWER_UNKNOWN = 2,
};

static const size_t max_question_size         = 64;
static const size_t akinator_container_size   = 64;

static akinator_error_t akinator_ask_question               (akinator_t         *akinator,
                                                             akinator_node_t   **current_node);

static akinator_error_t akinator_read_answer                (akinator_answer_t  *answer);

static akinator_error_t akinator_handle_answer_yes          (akinator_node_t   **current_node);

static akinator_error_t akinator_handle_answer_no           (akinator_t         *akinator,
                                                             akinator_node_t   **current_node);

static akinator_error_t akinator_handle_new_object          (akinator_t         *akinator,
                                                             akinator_node_t   **current_node);

static akinator_error_t akinator_init_new_object_children   (akinator_t         *akinator,
                                                             akinator_node_t   **current_node);

static akinator_error_t akinator_read_new_object_questions  (akinator_node_t    *node);

static akinator_error_t akinator_read_database              (akinator_t         *akinator,
                                                             const char         *database_filename);

static akinator_error_t akinator_database_read_node         (akinator_t         *akinator,
                                                             akinator_node_t    *node);

static akinator_error_t akinator_database_clean_buffer      (akinator_t         *akinator);

static akinator_error_t akinator_database_read_children     (akinator_t         *akinator,
                                                             akinator_node_t    *node);

static akinator_error_t akinator_update_database            (akinator_t         *akinator);

static akinator_error_t akinator_database_write_node        (akinator_node_t    *node,
                                                             FILE               *database,
                                                             size_t              level);

static akinator_error_t akinator_database_move_quotes       (akinator_t         *akinator);

static akinator_error_t akinator_database_read_question     (akinator_t         *akinator,
                                                             akinator_node_t    *node);

static akinator_error_t akinator_check_if_new_node          (akinator_t         *akinator);

static akinator_error_t akinator_check_if_node_end          (akinator_t         *akinator);

static akinator_error_t akinator_database_read_file         (akinator_t         *akinator,
                                                             const char         *database_filename);

static akinator_error_t akinator_try_rewrite_database       (akinator_t         *akinator);

static akinator_error_t akinator_get_free_node              (akinator_t         *akinator,
                                                             akinator_node_t   **node);

akinator_error_t akinator_ctor(akinator_t *akinator, const char *database_filename) {
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);

    akinator->container_size = akinator_container_size;
    akinator->database_name = database_filename;

    akinator_error_t error_code = AKINATOR_SUCCESS;
    if((error_code = akinator_read_database(akinator,
                                            database_filename)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    if((error_code = akinator_dump_init(akinator)) != AKINATOR_SUCCESS) {
        return error_code;
    }

    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_guess(akinator_t *akinator) {
    akinator_node_t *current_node = akinator->root;
    akinator_error_t error_code = AKINATOR_SUCCESS;

    while(true) {
        error_code = akinator_ask_question(akinator, &current_node);
        if(error_code == AKINATOR_EXIT_SUCCESS) {
            return AKINATOR_SUCCESS;
        }
        if(error_code != AKINATOR_SUCCESS) {
            return error_code;
        }
    }
}

akinator_error_t akinator_dtor(akinator_t *akinator) {
    for(size_t element = 0; element < akinator->containers_number; element++) {
        free(akinator->containers[element]);
    }
    text_buffer_dtor(&akinator->new_questions_storage);
    fclose(akinator->general_dump);
    free(akinator->old_questions_storage);

    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_read_database(akinator_t *akinator, const char *database_filename) {
    akinator_error_t error_code = AKINATOR_SUCCESS;
    if((error_code = akinator_database_read_file(akinator, database_filename)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    if((error_code = text_buffer_ctor(&akinator->new_questions_storage,
                                      max_question_size,
                                      akinator->old_storage_size)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    if((error_code = akinator_get_free_node(akinator,
                                            &akinator->root)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    if((error_code = akinator_database_read_node(akinator,
                                                 akinator->root)) != AKINATOR_SUCCESS) {
        return error_code;
    }

    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_database_read_file(akinator_t *akinator, const char *database_filename) {
    FILE *database = fopen(database_filename, "rb");
    if(database == NULL) {
        color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                     "Error while opening database.\n");
        return AKINATOR_DATABASE_OPENING_ERROR;
    }

    akinator->old_storage_size = file_size(database);
    akinator->old_questions_storage = (char *)calloc(akinator->old_storage_size + 1,
                                                     sizeof(char));
    if(akinator->old_questions_storage == NULL) {
        color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                     "Error while allocating old questions storage.\n");
        return AKINATOR_TEXT_BUFFER_ALLOCATION_ERROR;
    }

    if(fread(akinator->old_questions_storage,
             sizeof(char),
             akinator->old_storage_size,
             database) != akinator->old_storage_size) {
        color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                     "Error while reading database from file.\n");
        return AKINATOR_DATABASE_READING_ERROR;
    }

    fclose(database);
    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_database_read_node(akinator_t      *akinator,
                                             akinator_node_t *node) {
    akinator_error_t error_code = AKINATOR_SUCCESS;
    if((error_code = akinator_check_if_new_node(akinator)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    if((error_code = akinator_database_read_question(akinator,
                                                     node)) != AKINATOR_SUCCESS) {
        return error_code;
    }

    if(akinator->old_questions_storage[akinator->questions_storage_position++] != '}') {
        if((error_code = akinator_database_read_children(akinator,
                                                         node)) != AKINATOR_SUCCESS) {
            return error_code;
        }
        if((error_code = akinator_check_if_node_end(akinator)) != AKINATOR_SUCCESS) {
            return error_code;
        }
    }

    if((error_code = akinator_database_clean_buffer(akinator)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_check_if_new_node(akinator_t *akinator) {
    if(akinator->old_questions_storage[akinator->questions_storage_position++] != '{') {
        color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                     "Error while reading database.\n");
        return AKINATOR_DATABASE_READING_ERROR;
    }

    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_check_if_node_end(akinator_t *akinator) {
    if(akinator->old_questions_storage[akinator->questions_storage_position++] != '}') {
        color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                     "Error while reading database.\n");
        return AKINATOR_DATABASE_READING_ERROR;
    }

    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_database_read_question(akinator_t *akinator, akinator_node_t *node) {
    akinator_error_t error_code = AKINATOR_SUCCESS;
    if((error_code = akinator_database_move_quotes(akinator)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    node->question = akinator->old_questions_storage + akinator->questions_storage_position;
    if((error_code = akinator_database_move_quotes(akinator)) != AKINATOR_SUCCESS) {
        return error_code;
    }

    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_database_move_quotes(akinator_t *akinator) {
    while(akinator->old_questions_storage[akinator->questions_storage_position] != '\"') {
        if(akinator->old_questions_storage[akinator->questions_storage_position] == '\0') {
            color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                         "Error while reading database.\n");
            return AKINATOR_DATABASE_READING_ERROR;
        }
        akinator->questions_storage_position++;
    }

    akinator->old_questions_storage[akinator->questions_storage_position++] = '\0';
    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_database_read_children(akinator_t      *akinator,
                                                 akinator_node_t *node) {
    akinator_error_t error_code = AKINATOR_SUCCESS;

    if((error_code = akinator_get_free_node(akinator,
                                            &node->no)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    if((error_code = akinator_get_free_node(akinator,
                                            &node->yes)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    if((error_code = akinator_database_clean_buffer(akinator)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    if((error_code = akinator_database_read_node(akinator,
                                                 node->yes)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    if((error_code = akinator_database_read_node(akinator,
                                                 node->no)) != AKINATOR_SUCCESS) {
        return error_code;
    }

    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_database_clean_buffer(akinator_t *akinator) {
    char symbol = akinator->old_questions_storage[akinator->questions_storage_position++];
    while(!isgraph(symbol) && symbol != '\0') {
        symbol = akinator->old_questions_storage[akinator->questions_storage_position++];
    }
    akinator->questions_storage_position--;
    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_get_free_node(akinator_t       *akinator,
                                        akinator_node_t **node) {
    if(akinator->containers_number * akinator->container_size == akinator->used_storage) {
        akinator_node_t *new_container = (akinator_node_t *)calloc(akinator->container_size,
                                                                   sizeof(akinator_node_t));
        if(new_container == NULL) {
            color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                         "Error while allocating tree storage.\n");
            return AKINATOR_TREE_ALLOCATION_ERROR;
        }
        akinator->containers[akinator->containers_number] = new_container;
        akinator->containers_number++;
    }

    size_t container_number = akinator->used_storage / akinator->container_size;
    size_t container_index  = akinator->used_storage % akinator->container_size;
    *node = akinator->containers[container_number] + container_index;

    akinator->used_storage++;
    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_ask_question(akinator_t       *akinator,
                                       akinator_node_t **current_node) {
    color_printf(MAGENTA_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                 "Это %s?\n",
                 (*current_node)->question);
    akinator_error_t error_code = AKINATOR_SUCCESS;
    akinator_answer_t answer = AKINATOR_ANSWER_UNKNOWN;
    if((error_code = akinator_read_answer(&answer)) != AKINATOR_SUCCESS) {
        return error_code;
    }

    switch(answer) {
        case AKINATOR_ANSWER_YES: {
            return akinator_handle_answer_yes(current_node);
        }
        case AKINATOR_ANSWER_NO: {
            return akinator_handle_answer_no(akinator, current_node);
        }
        case AKINATOR_ANSWER_UNKNOWN: {
            color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                         "Не понял ты чё не русский.\n");
            return AKINATOR_SUCCESS;
        }
        default: {
            color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                         "Error while getting user answer.\n");
            return AKINATOR_USER_ANSWER_ERROR;
        }
    }
}

akinator_error_t akinator_read_answer(akinator_answer_t *answer) {
    char text_answer[16] = {};
    scanf("%s", text_answer);
    if(strcmp(text_answer, "да") == 0) {
        *answer = AKINATOR_ANSWER_YES;
    }
    else if(strcmp(text_answer, "нет") == 0) {
        *answer = AKINATOR_ANSWER_NO;
    }
    else {
        *answer = AKINATOR_ANSWER_UNKNOWN;
    }

    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_handle_answer_yes(akinator_node_t **current_node) {
    if(is_leaf(*current_node)) {
        color_printf(GREEN_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                     "Ну вот я опять отгадал а ты слаб.\n");
        return AKINATOR_EXIT_SUCCESS;
    }
    else {
        *current_node = (*current_node)->yes;
        return AKINATOR_SUCCESS;
    }
}

akinator_error_t akinator_handle_answer_no(akinator_t       *akinator,
                                           akinator_node_t **current_node) {
    if(is_leaf(*current_node)) {
        return akinator_handle_new_object(akinator, current_node);
    }
    else {
        *current_node = (*current_node)->no;
        return AKINATOR_SUCCESS;
    }
}

akinator_error_t akinator_handle_new_object(akinator_t       *akinator,
                                            akinator_node_t **current_node) {
    akinator_error_t error_code = AKINATOR_SUCCESS;

    if((error_code = akinator_init_new_object_children(akinator,
                                                       current_node)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    if((error_code = akinator_read_new_object_questions(*current_node)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    if((error_code = akinator_try_rewrite_database(akinator)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    return AKINATOR_EXIT_SUCCESS;
}

akinator_error_t akinator_read_new_object_questions(akinator_node_t *node) {
    akinator_node_t *node_no  = node->no;
    akinator_node_t *node_yes = node->yes;

    color_printf(MAGENTA_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                 "Скажи ково или что ты загадал по братке.\n");
    scanf("\n%[^\n]", node_yes->question);

    color_printf(MAGENTA_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                 "А что его отличает от %sа?\n",
                 node_no->question);
    scanf("\n%[^\n]", node->question);
    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_try_rewrite_database(akinator_t *akinator) {
    akinator_error_t error_code = AKINATOR_SUCCESS;
    color_printf(BLUE_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                 "Ты хочешь чтобы я обновил свою базу данных?\n");
    while(true) {
        akinator_answer_t answer = AKINATOR_ANSWER_UNKNOWN;
        if((error_code = akinator_read_answer(&answer)) != AKINATOR_SUCCESS) {
            return error_code;
        }
        switch(answer) {
            case AKINATOR_ANSWER_YES: {
                return akinator_update_database(akinator);
            }
            case AKINATOR_ANSWER_NO: {
                return AKINATOR_EXIT_SUCCESS;
            }
            case AKINATOR_ANSWER_UNKNOWN: {
                color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                             "Не понял ты чё не русский.\n");
                break;
            }
            default: {
                color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                             "Error while getting user answer.\n");
                return AKINATOR_USER_ANSWER_ERROR;
            }
        }
    }
}

akinator_error_t akinator_init_new_object_children(akinator_t       *akinator,
                                                   akinator_node_t **current_node) {
    akinator_error_t error_code = AKINATOR_SUCCESS;
    if((error_code = akinator_get_free_node(akinator,
                                            &(*current_node)->no )) != AKINATOR_SUCCESS) {
        return error_code;
    }
    if((error_code = akinator_get_free_node(akinator,
                                            &(*current_node)->yes)) != AKINATOR_SUCCESS) {
        return error_code;
    }

    (*current_node)->no->question = (*current_node)->question;
    if((error_code = text_buffer_add(&akinator->new_questions_storage,
                                     &(*current_node)->question)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    if((error_code = text_buffer_add(&akinator->new_questions_storage,
                                     &(*current_node)->yes->question)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_update_database(akinator_t *akinator) {
    FILE *database = fopen(akinator->database_name, "w");
    if(database == NULL) {
        color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                     "Error while opening database.\n");
        return AKINATOR_DATABASE_OPENING_ERROR;
    }

    akinator_error_t error_code = AKINATOR_SUCCESS;
    if((error_code = akinator_database_write_node(akinator->root,
                                                  database,
                                                  0)) != AKINATOR_SUCCESS) {
        return error_code;
    }

    return AKINATOR_EXIT_SUCCESS;
}

akinator_error_t akinator_database_write_node(akinator_node_t *node,
                                              FILE            *database,
                                              size_t level) {
    for(size_t i = 0; i < level; i++) {
        fputc('\t', database);
    }
    fprintf(database, "{\"%s\"", node->question);
    if(is_leaf(node)) {
        fputs("}\n", database);
        return AKINATOR_SUCCESS;
    }

    fputc('\n', database);

    akinator_error_t error_code = AKINATOR_SUCCESS;
    if((error_code = akinator_database_write_node(node->yes,
                                                  database,
                                                  level + 1)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    if((error_code = akinator_database_write_node(node->no,
                                                  database,
                                                  level + 1)) != AKINATOR_SUCCESS) {
        return error_code;
    }

    fputs("}\n", database);
    return AKINATOR_SUCCESS;
}

