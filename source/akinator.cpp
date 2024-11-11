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
#include "custom_assert.h"

/*=============================================================================*/

enum akinator_answer_t {
    AKINATOR_ANSWER_YES     = 0,
    AKINATOR_ANSWER_NO      = 1,
    AKINATOR_ANSWER_UNKNOWN = 2,
};

/*=============================================================================*/

static const size_t max_question_size         = 64;
static const size_t akinator_container_size   = 64;

/*=============================================================================*/

static akinator_error_t akinator_ask_question               (akinator_t         *akinator,
                                                             akinator_node_t   **current_node);

static akinator_error_t akinator_read_answer                (akinator_answer_t  *answer);

static akinator_error_t akinator_handle_answer_yes          (akinator_node_t   **current_node);

static akinator_error_t akinator_handle_answer_no           (akinator_t         *akinator,
                                                             akinator_node_t   **current_node);

static akinator_error_t akinator_handle_new_object          (akinator_t         *akinator,
                                                             akinator_node_t    *current_node);

static akinator_error_t akinator_init_new_object_children   (akinator_t         *akinator,
                                                             akinator_node_t    *current_node);

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

static akinator_error_t akinator_find_node                  (akinator_t         *akinator,
                                                             const char         *object,
                                                             akinator_node_t   **node_output);

static akinator_error_t akinator_print_definition           (akinator_node_t   **definition_elements,
                                                             size_t              level);

static akinator_error_t akinator_print_definition_element   (akinator_node_t   **way,
                                                             size_t              index);

static akinator_error_t akinator_read_and_find              (akinator_t         *akinator,
                                                             size_t             *level_output,
                                                             akinator_node_t   **way,
                                                             const char         *question);

static akinator_error_t akinator_print_difference           (size_t              level_first,
                                                             akinator_node_t   **way_first,
                                                             size_t              level_second,
                                                             akinator_node_t   **way_second);

static akinator_error_t akinator_leafs_array_init           (akinator_t         *akinator,
                                                             size_t              capacity);

static akinator_error_t akinator_leafs_array_add            (akinator_t         *akinator,
                                                             akinator_node_t    *node);

static akinator_error_t akinator_get_children_free_nodes    (akinator_t         *akinator,
                                                             akinator_node_t    *parent);

static akinator_error_t akinator_verify_children_parent     (akinator_node_t *node);

/*=============================================================================*/

#define RETURN_IF_ERROR(...) {   /*FUNCTION CALL ONLY*/          \
    akinator_error_t __error_code = __VA_ARGS__;                 \
    if(__error_code != AKINATOR_SUCCESS) {                       \
        return __error_code;                                     \
    }                                                            \
}

#define AKINATOR_VERIFY(__akinator) {                            \
    akinator_error_t __error_code = akinator_verify(__akinator); \
    if(__error_code != AKINATOR_SUCCESS) {                       \
        return __error_code;                                     \
    }                                                            \
}

/*=============================================================================*/

akinator_error_t akinator_ctor(akinator_t *akinator, const char *database_filename) {
    SetConsoleCP      (1251);
    SetConsoleOutputCP(1251);
    _C_ASSERT(akinator          != NULL, return AKINATOR_NULL_POINTER          );
    _C_ASSERT(database_filename != NULL, return AKINATOR_DATABASE_FILENAME_NULL);

    akinator->container_size = akinator_container_size;
    akinator->database_name  = database_filename;

    RETURN_IF_ERROR(akinator_read_database(akinator,
                                           database_filename));

    RETURN_IF_ERROR(akinator_dump_init    (akinator));

    AKINATOR_VERIFY(akinator);
    return AKINATOR_SUCCESS;
}

/*=============================================================================*/

akinator_error_t akinator_guess(akinator_t *akinator) {
    AKINATOR_VERIFY(akinator);
    akinator_node_t  *current_node = akinator->root;

    while(true) {
        akinator_error_t error_code = akinator_ask_question(akinator, &current_node);
        if(error_code == AKINATOR_EXIT_SUCCESS) {
            AKINATOR_VERIFY(akinator);
            return AKINATOR_SUCCESS;
        }
        if(error_code != AKINATOR_SUCCESS     ) {
            return error_code;
        }
    }
}

/*=============================================================================*/

akinator_error_t akinator_dtor(akinator_t *akinator) {
    _C_ASSERT(akinator != NULL, return AKINATOR_NULL_POINTER);

    for(size_t element = 0; element < akinator->containers_number; element++) {
        free(akinator->containers[element]);
    }

    text_buffer_dtor(&akinator->new_questions_storage);
    fclose          (akinator->general_dump);
    free            (akinator->old_questions_storage);
    free            (akinator->leafs_array);

    memset(akinator, 0, sizeof(*akinator));
    return AKINATOR_SUCCESS;
}

/*=============================================================================*/

akinator_error_t akinator_difference(akinator_t *akinator) {
    AKINATOR_VERIFY(akinator);
    akinator_node_t **way_first = (akinator_node_t **)calloc((akinator->used_storage + 1) * 2,
                                                             sizeof(akinator_node_t *));
    if(way_first == NULL) {
        color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                     "Error while allocating memory to definition elements.\n");
        return AKINATOR_DEFINITION_ALLOCATING_ERROR;
    }
    akinator_node_t **way_second = way_first + akinator->used_storage + 1;
    size_t level_first = 0;
    size_t level_second = 0;

    akinator_error_t error_code = AKINATOR_SUCCESS;
    if((error_code = akinator_read_and_find(akinator,
                                            &level_first,
                                            way_first,
                                            "Кто первый в сравнении?\n")) != AKINATOR_SUCCESS) {
        free(way_first);
        return error_code;
    }
    if((error_code = akinator_read_and_find(akinator,
                                            &level_second,
                                            way_second,
                                            "Кто второй в сравнении?\n")) != AKINATOR_SUCCESS) {
        free(way_first);
        return error_code;
    }

    if((error_code = akinator_print_difference(level_first,
                                               way_first,
                                               level_second,
                                               way_second)) != AKINATOR_SUCCESS) {
        free(way_first);
        return error_code;
    }

    free(way_first);
    AKINATOR_VERIFY(akinator);
    return AKINATOR_SUCCESS;
}

/*=============================================================================*/

akinator_error_t akinator_definition(akinator_t *akinator) {
    AKINATOR_VERIFY(akinator);
    akinator_node_t **way = (akinator_node_t **)calloc(akinator->used_storage + 1,
                                                       sizeof(akinator_node_t *));
    size_t level = 0;
    if(way == NULL) {
        color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                     "Error while allocating memory to definition elements.\n");
        return AKINATOR_DEFINITION_ALLOCATING_ERROR;
    }

    akinator_error_t error_code = AKINATOR_SUCCESS;
    if((error_code = akinator_read_and_find(akinator,
                                            &level,
                                            way,
                                            "Определение кого ты хочешь получить?\n")) != AKINATOR_SUCCESS) {
        free(way);
        return error_code;
    }
    if((error_code = akinator_print_definition(way, level)) != AKINATOR_SUCCESS) {
        free(way);
        return error_code;
    }

    free(way);
    AKINATOR_VERIFY(akinator);
    return AKINATOR_SUCCESS;
}

/*=============================================================================*/

akinator_error_t akinator_read_database(akinator_t *akinator,
                                        const char *database_filename) {
    _C_ASSERT(database_filename != NULL, return AKINATOR_DATABASE_FILENAME_NULL);
    AKINATOR_VERIFY(akinator);

    RETURN_IF_ERROR(akinator_database_read_file (akinator,
                                                 database_filename));

    RETURN_IF_ERROR(text_buffer_ctor            (&akinator->new_questions_storage,
                                                 max_question_size,
                                                 akinator->old_storage_size));

    RETURN_IF_ERROR(akinator_leafs_array_init   (akinator,
                                                 akinator->old_storage_size));

    RETURN_IF_ERROR(akinator_get_free_node      (akinator,
                                                 &akinator->root));

    RETURN_IF_ERROR(akinator_database_read_node (akinator,
                                                 akinator->root));

    AKINATOR_VERIFY(akinator);
    return AKINATOR_SUCCESS;
}

/*=============================================================================*/

akinator_error_t akinator_database_read_file(akinator_t *akinator,
                                             const char *database_filename) {
    _C_ASSERT(database_filename != NULL, return AKINATOR_DATABASE_FILENAME_NULL);
    AKINATOR_VERIFY(akinator);

    FILE *database = fopen(database_filename, "rb");
    if(database == NULL) {
        color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                     "Error while opening database.\n");
        return AKINATOR_DATABASE_OPENING_ERROR;
    }

    akinator->old_storage_size      = file_size(database);
    akinator->old_questions_storage = (char *)calloc(akinator->old_storage_size + 1,
                                                     sizeof(char));

    if(akinator->old_questions_storage == NULL) {
        color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                     "Error while allocating old questions storage.\n");
        fclose(database);
        return AKINATOR_TEXT_BUFFER_ALLOCATION_ERROR;
    }

    if(fread(akinator->old_questions_storage,
             sizeof(char),
             akinator->old_storage_size,
             database) != akinator->old_storage_size) {
        color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                     "Error while reading database from file.\n");
        fclose(database);
        return AKINATOR_DATABASE_READING_ERROR;
    }

    fclose(database);
    AKINATOR_VERIFY(akinator);
    return AKINATOR_SUCCESS;
}

/*=============================================================================*/

akinator_error_t akinator_database_read_node(akinator_t      *akinator,
                                             akinator_node_t *node) {
    _C_ASSERT(akinator != NULL, return AKINATOR_NULL_POINTER);
    _C_ASSERT(node     != NULL, return AKINATOR_NODE_NULL   );

    RETURN_IF_ERROR(akinator_check_if_new_node     (akinator));
    RETURN_IF_ERROR(akinator_database_read_question(akinator, node));

    if(akinator->old_questions_storage[akinator->questions_storage_position++] != '}') {
        RETURN_IF_ERROR(akinator_database_read_children(akinator, node));
        RETURN_IF_ERROR(akinator_check_if_node_end     (akinator));
    }
    else {
        RETURN_IF_ERROR(akinator_leafs_array_add       (akinator, node));
    }

    RETURN_IF_ERROR(akinator_database_clean_buffer(akinator));
    return AKINATOR_SUCCESS;
}

/*=============================================================================*/

akinator_error_t akinator_check_if_new_node(akinator_t *akinator) {
    _C_ASSERT(akinator != NULL, return AKINATOR_NULL_POINTER);

    if(akinator->old_questions_storage[akinator->questions_storage_position++] != '{') {
        color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                     "Error while reading database.\n");
        return AKINATOR_DATABASE_READING_ERROR;
    }

    return AKINATOR_SUCCESS;
}

/*=============================================================================*/

akinator_error_t akinator_check_if_node_end(akinator_t *akinator) {
    _C_ASSERT(akinator != NULL, return AKINATOR_NULL_POINTER);

    if(akinator->old_questions_storage[akinator->questions_storage_position++] != '}') {
        color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                     "Error while reading database.\n");
        return AKINATOR_DATABASE_READING_ERROR;
    }

    return AKINATOR_SUCCESS;
}

/*=============================================================================*/

akinator_error_t akinator_database_read_question(akinator_t *akinator, akinator_node_t *node) {
    AKINATOR_VERIFY(akinator);
    RETURN_IF_ERROR(akinator_database_move_quotes(akinator));
    node->question = akinator->old_questions_storage + akinator->questions_storage_position;
    RETURN_IF_ERROR(akinator_database_move_quotes(akinator));
    AKINATOR_VERIFY(akinator);
    return AKINATOR_SUCCESS;
}

/*=============================================================================*/

akinator_error_t akinator_database_move_quotes(akinator_t *akinator) {
    _C_ASSERT(akinator != NULL, return AKINATOR_NULL_POINTER);

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

/*=============================================================================*/

akinator_error_t akinator_database_read_children(akinator_t      *akinator,
                                                 akinator_node_t *node) {
    _C_ASSERT(node != NULL, return AKINATOR_NODE_NULL);
    AKINATOR_VERIFY(akinator);

    RETURN_IF_ERROR(akinator_get_children_free_nodes(akinator, node));

    node->no->parent  = node;
    node->yes->parent = node;

    RETURN_IF_ERROR(akinator_database_clean_buffer  (akinator));
    RETURN_IF_ERROR(akinator_database_read_node     (akinator, node->yes));
    RETURN_IF_ERROR(akinator_database_read_node     (akinator, node->no));

    AKINATOR_VERIFY(akinator);
    return AKINATOR_SUCCESS;
}

/*=============================================================================*/

akinator_error_t akinator_database_clean_buffer(akinator_t *akinator) {
    _C_ASSERT(akinator != NULL, return AKINATOR_NULL_POINTER);

    char symbol = akinator->old_questions_storage[akinator->questions_storage_position++];
    while(symbol != '{' && symbol != '}' && symbol != '\0') {
        symbol = akinator->old_questions_storage[akinator->questions_storage_position++];
    }

    akinator->questions_storage_position--;

    return AKINATOR_SUCCESS;
}

/*=============================================================================*/

akinator_error_t akinator_get_free_node(akinator_t       *akinator,
                                        akinator_node_t **node) {
    _C_ASSERT(akinator != NULL, return AKINATOR_NULL_POINTER);
    _C_ASSERT(node     != NULL, return AKINATOR_NODE_NULL   );

    if(akinator->containers_number * akinator->container_size == akinator->used_storage) {
        akinator_node_t *new_container = (akinator_node_t *)calloc(akinator->container_size,
                                                                   sizeof(akinator_node_t));
        if(new_container == NULL) {
            color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                         "Error while allocating tree storage.\n");
            return AKINATOR_TREE_ALLOCATION_ERROR;
        }
        akinator->containers[akinator->containers_number++] = new_container;
    }

    size_t container_number = akinator->used_storage / akinator->container_size;
    size_t container_index  = akinator->used_storage % akinator->container_size;
    *node = akinator->containers[container_number] + container_index;
    akinator->used_storage++;

    return AKINATOR_SUCCESS;
}

/*=============================================================================*/

akinator_error_t akinator_ask_question(akinator_t       *akinator,
                                       akinator_node_t **current_node) {
    _C_ASSERT(current_node  != NULL, return AKINATOR_NODE_NULL);
    _C_ASSERT(*current_node != NULL, return AKINATOR_NODE_NULL);
    AKINATOR_VERIFY(akinator);

    color_printf(MAGENTA_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                 "Это %s?\n", (*current_node)->question);

    akinator_answer_t answer = AKINATOR_ANSWER_UNKNOWN;
    RETURN_IF_ERROR(akinator_read_answer(&answer));

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

/*=============================================================================*/

akinator_error_t akinator_read_answer(akinator_answer_t *answer) {
    _C_ASSERT(answer != NULL, return AKINATOR_ANSWER_NULL);

    char text_answer[max_question_size] = {};
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

/*=============================================================================*/

akinator_error_t akinator_handle_answer_yes(akinator_node_t **current_node) {
    _C_ASSERT(current_node  != NULL, return AKINATOR_NODE_NULL);
    _C_ASSERT(*current_node != NULL, return AKINATOR_NODE_NULL);

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

/*=============================================================================*/

akinator_error_t akinator_handle_answer_no(akinator_t       *akinator,
                                           akinator_node_t **current_node) {
    AKINATOR_VERIFY(akinator);

    if(is_leaf(*current_node)) {
        return akinator_handle_new_object(akinator, *current_node);
    }
    else {
        *current_node = (*current_node)->no;
        return AKINATOR_SUCCESS;
    }
}

/*=============================================================================*/

akinator_error_t akinator_handle_new_object(akinator_t       *akinator,
                                            akinator_node_t *current_node) {
    AKINATOR_VERIFY(akinator);

    RETURN_IF_ERROR(akinator_init_new_object_children (akinator, current_node));
    RETURN_IF_ERROR(akinator_read_new_object_questions(current_node));
    RETURN_IF_ERROR(akinator_try_rewrite_database     (akinator));
    return AKINATOR_EXIT_SUCCESS;
}

/*=============================================================================*/

akinator_error_t akinator_read_new_object_questions(akinator_node_t *node) {
    _C_ASSERT(node != NULL, return AKINATOR_NODE_NULL);

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

/*=============================================================================*/

akinator_error_t akinator_try_rewrite_database(akinator_t *akinator) {
    AKINATOR_VERIFY(akinator);

    color_printf(BLUE_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                 "Ты хочешь чтобы я обновил свою базу данных?\n");
    while(true) {
        akinator_answer_t answer = AKINATOR_ANSWER_UNKNOWN;
        RETURN_IF_ERROR(akinator_read_answer(&answer));

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

/*=============================================================================*/

akinator_error_t akinator_init_new_object_children(akinator_t       *akinator,
                                                   akinator_node_t  *current_node) {
    _C_ASSERT(current_node  != NULL, return AKINATOR_NODE_NULL);
    AKINATOR_VERIFY(akinator);

    RETURN_IF_ERROR(akinator_get_children_free_nodes(akinator, current_node));

    akinator_node_t *child_no  = current_node->no;
    akinator_node_t *child_yes = current_node->yes;

    RETURN_IF_ERROR(akinator_leafs_array_add        (akinator, child_yes));

    child_no ->parent = current_node;
    child_yes->parent = current_node;

    for(size_t index = 0; index < akinator->leafs_array_size; index++) {
        if(akinator->leafs_array[index] == current_node) {
            akinator->leafs_array[index] = child_no;
        }
    }
    child_no->question = current_node->question;

    RETURN_IF_ERROR(text_buffer_add(&akinator->new_questions_storage, &current_node->question));
    RETURN_IF_ERROR(text_buffer_add(&akinator->new_questions_storage, &child_yes->question));

    AKINATOR_VERIFY(akinator);
    return AKINATOR_SUCCESS;
}

/*=============================================================================*/

akinator_error_t akinator_update_database(akinator_t *akinator) {
    AKINATOR_VERIFY(akinator);

    FILE *database = fopen(akinator->database_name, "w");
    if(database == NULL) {
        color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                     "Error while opening database.\n");
        return AKINATOR_DATABASE_OPENING_ERROR;
    }

    akinator_error_t error_code = akinator_database_write_node(akinator->root, database, 0);
    fclose(database);
    if(error_code != AKINATOR_SUCCESS) {
        return error_code;
    }

    AKINATOR_VERIFY(akinator);
    return AKINATOR_EXIT_SUCCESS;
}

/*=============================================================================*/

akinator_error_t akinator_database_write_node(akinator_node_t *node,
                                              FILE            *database,
                                              size_t level) {
    _C_ASSERT(node     != NULL, return AKINATOR_NODE_NULL             );
    _C_ASSERT(database != NULL, return AKINATOR_DATABASE_OPENING_ERROR);

    for(size_t i = 0; i < level; i++) {
        fputc('\t', database);
    }
    fprintf(database, "{\"%s\"", node->question);
    if(is_leaf(node)) {
        fputs("}\n", database);
        return AKINATOR_SUCCESS;
    }

    fputc('\n', database);

    RETURN_IF_ERROR(akinator_database_write_node(node->yes, database, level + 1));
    RETURN_IF_ERROR(akinator_database_write_node(node->no,  database, level + 1));

    for(size_t i = 0; i < level; i++) {
        fputc('\t', database);
    }
    fputs("}\n", database);
    return AKINATOR_SUCCESS;
}

/*=============================================================================*/

akinator_error_t akinator_read_and_find(akinator_t        *akinator,
                                        size_t            *level_output,
                                        akinator_node_t  **way,
                                        const char        *question) {
    _C_ASSERT(level_output != NULL, return AKINATOR_INVALID_NODE_LEVEL);
    _C_ASSERT(way          != NULL, return AKINATOR_WAY_ARRAY_NULL    );
    _C_ASSERT(question     != NULL, return AKINATOR_NULL_OBJECT_NAME  );
    AKINATOR_VERIFY(akinator);

    akinator_node_t *node = NULL;
    color_printf(MAGENTA_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND, "%s", question);
    while(true) {
        char object[max_question_size] = {};
        scanf("\n%[^\n]", object);


        akinator_error_t error_code = akinator_find_node(akinator, object, &node);
        if(error_code == AKINATOR_SUCCESS) {
            color_printf(GREEN_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                         "Такова не знаю\n");
            continue;
        }
        if(error_code == AKINATOR_EXIT_SUCCESS) {
            break;
        }
        return error_code;
    }

    for(size_t element = 0; node != NULL; element++) {
        way[element] = node;
        node = node->parent;
        (*level_output)++;
    }
    AKINATOR_VERIFY(akinator);
    return AKINATOR_SUCCESS;
}

/*=============================================================================*/

akinator_error_t akinator_print_definition(akinator_node_t **definition_elements,
                                           size_t            level) {
    _C_ASSERT(definition_elements != NULL, return AKINATOR_WAY_ARRAY_NULL    );
    _C_ASSERT(level               >= 1   , return AKINATOR_INVALID_NODE_LEVEL);

    color_printf(GREEN_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                 "%s - это ", definition_elements[0]->question);

    if(definition_elements[level - 1]->no == definition_elements[level - 2]) {
        color_printf(GREEN_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND, "не ");
    }
    color_printf(GREEN_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                 "%s, который(ая) ", definition_elements[level - 1]->question);

    for(size_t element = level - 2; element > 0; element--) {
        RETURN_IF_ERROR(akinator_print_definition_element(definition_elements, element));
    }
    fputc('\n', stdout);
    return AKINATOR_SUCCESS;
}

/*=============================================================================*/

akinator_error_t akinator_find_node(akinator_t       *akinator,
                                    const char       *object,
                                    akinator_node_t **node_output) {
    _C_ASSERT(object      != NULL, return AKINATOR_NULL_OBJECT_NAME);
    _C_ASSERT(node_output != NULL, return AKINATOR_NODE_NULL       );
    AKINATOR_VERIFY(akinator);

    for(size_t index = 0; index < akinator->leafs_array_size; index++) {
        akinator_node_t *node = akinator->leafs_array[index];
        if(node != NULL && strcmp(node->question, object) == 0) {
            *node_output = node;
            return AKINATOR_EXIT_SUCCESS;
        }
    }

    AKINATOR_VERIFY(akinator);
    return AKINATOR_SUCCESS;
}

/*=============================================================================*/

akinator_error_t akinator_print_difference(size_t            level_first,
                                           akinator_node_t **way_first,
                                           size_t            level_second,
                                           akinator_node_t **way_second) {
    _C_ASSERT(way_first    != NULL, return AKINATOR_WAY_ARRAY_NULL    );
    _C_ASSERT(way_second   != NULL, return AKINATOR_WAY_ARRAY_NULL    );
    _C_ASSERT(level_first  >= 1   , return AKINATOR_INVALID_NODE_LEVEL);
    _C_ASSERT(level_second >= 1   , return AKINATOR_INVALID_NODE_LEVEL);

    size_t index_first  = level_first  - 1;
    size_t index_second = level_second - 1;
    if(way_first[index_first - 1] == way_second[index_second - 1]) {
        color_printf(GREEN_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                     "Они оба ");
    }
    else {
        color_printf(GREEN_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                     "%s ", way_first[0]->question);
    }
    while(way_first[index_first - 1] == way_second[index_second - 1] && index_first  > 0 && index_second > 0) {
        RETURN_IF_ERROR(akinator_print_definition_element(way_first, index_first));
        index_first--;
        index_second--;
    }

    if(way_first[level_first - 2] == way_second[level_second - 2]) {
        color_printf(GREEN_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                     "но %s ", way_first[0]->question);
    }
    for(; index_first > 0; index_first--) {
        RETURN_IF_ERROR(akinator_print_definition_element(way_first, index_first));
    }

    color_printf(GREEN_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                 ", а %s ", way_second[0]->question);
    for(; index_second > 0; index_second--) {
        RETURN_IF_ERROR(akinator_print_definition_element(way_second, index_second));
    }

    fputc('\n', stdout);
    return AKINATOR_SUCCESS;
}

/*=============================================================================*/

akinator_error_t akinator_print_definition_element(akinator_node_t **way,
                                                   size_t            index) {
    _C_ASSERT(way   != NULL, return AKINATOR_WAY_ARRAY_NULL    );
    _C_ASSERT(index >= 1   , return AKINATOR_INVALID_NODE_LEVEL);

    if(way[index]->no == way[index - 1]) {
        color_printf(GREEN_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND, "не ");
    }
    color_printf(GREEN_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                 "%s", way[index]->question);
    if(index != 1) {
        color_printf(GREEN_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND, ", ");
    }

    return AKINATOR_SUCCESS;
}

/*=============================================================================*/

akinator_error_t akinator_leafs_array_init(akinator_t *akinator,
                                           size_t      capacity) {
    AKINATOR_VERIFY(akinator);

    akinator->leafs_array = (akinator_node_t **)calloc(capacity,
                                                       sizeof(akinator->leafs_array[0]));
    if(akinator->leafs_array == NULL) {
        color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                     "Error while allocating leafs array.\n");
        return AKINATOR_LEAFS_ALLOCATING_ERROR;
    }
    akinator->leafs_array_capacity = capacity;

    AKINATOR_VERIFY(akinator);
    return AKINATOR_SUCCESS;
}

/*=============================================================================*/

akinator_error_t akinator_leafs_array_add(akinator_t      *akinator,
                                          akinator_node_t *node) {
    _C_ASSERT(akinator != NULL, return AKINATOR_NULL_POINTER);
    _C_ASSERT(node     != NULL, return AKINATOR_NODE_NULL   );

    if(akinator->leafs_array_size >= akinator->leafs_array_capacity) {
        akinator_node_t **new_array = (akinator_node_t **)realloc(akinator->leafs_array,
                                                                  akinator->leafs_array_capacity * 2);
        if(new_array == NULL) {
            color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                         "Error while reallocating leafs array.\n");
            return AKINATOR_LEAFS_ALLOCATING_ERROR;
        }
        akinator_node_t **start_of_new_part = new_array + akinator->leafs_array_capacity;
        if(memset(start_of_new_part, 0,
                  akinator->leafs_array_capacity) != start_of_new_part) {
            color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                         "Error while setting reallocated memory part to zeros.\n");
            return AKINATOR_MEMSET_ERROR;
        }

        akinator->leafs_array = new_array;
        akinator->leafs_array_capacity *= 2;
    }

    akinator->leafs_array[akinator->leafs_array_size] = node;

    akinator->leafs_array_size++;

    return AKINATOR_SUCCESS;
}

/*=============================================================================*/

akinator_error_t akinator_get_children_free_nodes(akinator_t      *akinator,
                                                 akinator_node_t  *parent) {
    _C_ASSERT(akinator != NULL, return AKINATOR_NULL_POINTER);
    _C_ASSERT(parent   != NULL, return AKINATOR_NODE_NULL   );

    RETURN_IF_ERROR(akinator_get_free_node(akinator, &parent->no));
    RETURN_IF_ERROR(akinator_get_free_node(akinator, &parent->yes));

    return AKINATOR_SUCCESS;
}

/*=============================================================================*/

akinator_error_t akinator_verify(akinator_t *akinator) {
    if(akinator == NULL) {
        return AKINATOR_NULL_POINTER;
    }

    if(akinator->containers_number >= max_nodes_containers_number) {
        return AKINATOR_CONTAINERS_OVERFLOW;
    }

    for(size_t container = 0; container < akinator->containers_number; container++) {
        if(akinator->containers[container] == NULL) {
            return AKINATOR_NULL_USED_CONTAINER;
        }
    }
    for(size_t container = akinator->containers_number; container < max_nodes_containers_number; container++) {
        if(akinator->containers[container] != NULL) {
            return AKINATOR_NOT_NULL_UNUSED_CONTAINER;
        }
    }

    if(akinator->root == NULL && akinator->used_storage == 0) {
        return AKINATOR_SUCCESS;
    }
    if(akinator->root == NULL || akinator->used_storage == 0) {
        return AKINATOR_NULL_ROOT;
    }

    RETURN_IF_ERROR(akinator_verify_children_parent(akinator->root));

    return AKINATOR_SUCCESS;
}

/*=============================================================================*/

akinator_error_t akinator_verify_children_parent(akinator_node_t *node) {
    _C_ASSERT(node != NULL, return AKINATOR_NODE_NULL);

    akinator_node_t *child_no = node->no;
    akinator_node_t *child_yes = node->yes;
    if(child_no == NULL && child_yes == NULL) {
        return AKINATOR_SUCCESS;
    }
    if(child_no == NULL || child_yes == NULL) {
        return AKINATOR_ONE_CHILD;
    }

    if(child_no->parent != node || child_yes->parent != node) {
        return AKINATOR_CHILD_PARENT_CONNECTION_ERROR;
    }

    RETURN_IF_ERROR(akinator_verify_children_parent(child_no));
    RETURN_IF_ERROR(akinator_verify_children_parent(child_yes));
    return AKINATOR_SUCCESS;
}
