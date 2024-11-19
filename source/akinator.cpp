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
#include "graphics.h"

static const size_t MaxDefinitionSize     = 512;
static const size_t MaxQuestionSize       = 64;
static const size_t AkinatorContainerSize = 64;

/*=============================================================================*/

struct akinator_definition_t {
    char   definition[MaxDefinitionSize];
    size_t index;
};

/*=============================================================================*/

static akinator_error_t akinator_ask_question               (akinator_t            *akinator,
                                                             akinator_node_t      **current_node);

static akinator_error_t akinator_read_answer                (akinator_answer_t     *answer);

static akinator_error_t akinator_handle_answer_yes          (akinator_t            *akinator,
                                                             akinator_node_t      **current_node);

static akinator_error_t akinator_handle_answer_no           (akinator_t            *akinator,
                                                             akinator_node_t      **current_node);

static akinator_error_t akinator_handle_new_object          (akinator_t            *akinator,
                                                             akinator_node_t       *current_node);

static akinator_error_t akinator_init_new_object_children   (akinator_t            *akinator,
                                                             akinator_node_t       *current_node);

static akinator_error_t akinator_read_new_object_questions  (akinator_t            *akinator,
                                                             akinator_node_t       *node);

static akinator_error_t akinator_read_database              (akinator_t            *akinator,
                                                             const char            *db_filename);

static akinator_error_t akinator_database_read_node         (akinator_t            *akinator,
                                                             akinator_node_t       *node);

static akinator_error_t akinator_database_clean_buffer      (akinator_t            *akinator);

static akinator_error_t akinator_database_read_children     (akinator_t            *akinator,
                                                             akinator_node_t       *node);

static akinator_error_t akinator_update_database            (akinator_t            *akinator);

static akinator_error_t akinator_database_write_node        (akinator_node_t       *node,
                                                             FILE                  *database,
                                                             size_t                 level);

static akinator_error_t akinator_database_move_quotes       (akinator_t            *akinator);

static akinator_error_t akinator_database_read_question     (akinator_t            *akinator,
                                                             akinator_node_t       *node);

static akinator_error_t akinator_check_if_new_node          (akinator_t            *akinator);

static akinator_error_t akinator_check_if_node_end          (akinator_t            *akinator);

static akinator_error_t akinator_database_read_file         (akinator_t            *akinator,
                                                             const char            *db_filename);

static akinator_error_t akinator_try_rewrite_database       (akinator_t            *akinator);

static akinator_error_t akinator_get_free_node              (akinator_t            *akinator,
                                                             akinator_node_t      **node);

static akinator_error_t akinator_find_node                  (akinator_t            *akinator,
                                                             const char            *object,
                                                             akinator_node_t      **node_output);

static akinator_error_t akinator_print_definition           (akinator_t            *akinator,
                                                             akinator_node_t      **node_way,
                                                             size_t                 level);

static akinator_error_t akinator_print_definition_element   (akinator_node_t      **node_way,
                                                             size_t                 index,
                                                             akinator_definition_t *definition);

static akinator_error_t akinator_read_and_find              (akinator_t            *akinator,
                                                             size_t                *level_output,
                                                             akinator_node_t      **way,
                                                             const char            *question);

static akinator_error_t akinator_print_difference           (akinator_t            *akinator,
                                                             size_t                 level_first,
                                                             akinator_node_t      **way_first,
                                                             size_t                 level_second,
                                                             akinator_node_t      **way_second);

static akinator_error_t akinator_leafs_array_init           (akinator_t            *akinator,
                                                             size_t                 capacity);

static akinator_error_t akinator_leafs_array_add            (akinator_t            *akinator,
                                                             akinator_node_t       *node);

static akinator_error_t akinator_get_children_free_nodes    (akinator_t            *akinator,
                                                             akinator_node_t       *parent);

static akinator_error_t akinator_verify_children_parent     (akinator_node_t       *node);

static akinator_error_t akinator_print_message              (akinator_t            *akinator,
                                                             const char            *format, ...);

static akinator_error_t akinator_add_definition             (akinator_definition_t *definition,
                                                             const char            *format, ...);

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
    akinator_graphics_init();
    _C_ASSERT(akinator          != NULL, return AKINATOR_NULL_POINTER          );
    _C_ASSERT(database_filename != NULL, return AKINATOR_DATABASE_FILENAME_NULL);

    akinator->container_size = AkinatorContainerSize;
    akinator->database_name  = database_filename;

    if(tts_ctor(&akinator->tts) != TTS_SUCCESS) {
        return AKINATOR_TEXT_TO_SPEECH_ERROR;
    }

    RETURN_IF_ERROR(akinator_read_database(akinator,
                                           database_filename));

    RETURN_IF_ERROR(akinator_dump_init    (akinator));

    AKINATOR_VERIFY(akinator);
    return AKINATOR_SUCCESS;
}

/*=============================================================================*/

akinator_error_t akinator_guess(akinator_t *akinator) {
    AKINATOR_VERIFY(akinator);

    akinator_ui_set_guess();
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

    tts_dtor(&akinator->tts);

    text_buffer_dtor(&akinator->new_questions_storage);
    fclose          (akinator->general_dump);
    free            (akinator->old_questions_storage);
    free            (akinator->leafs_array);

    memset(akinator, 0, sizeof(*akinator));
    akinator_graphics_dtor();
    return AKINATOR_SUCCESS;
}

/*=============================================================================*/

akinator_error_t akinator_difference(akinator_t *akinator) {
    akinator_ui_set_difference();
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
                                            "Кто первый в сравнении?")) != AKINATOR_SUCCESS) {
        free(way_first);
        return error_code;
    }
    if((error_code = akinator_read_and_find(akinator,
                                            &level_second,
                                            way_second,
                                            "Кто второй в сравнении?")) != AKINATOR_SUCCESS) {
        free(way_first);
        return error_code;
    }

    if((error_code = akinator_print_difference(akinator,
                                               level_first,
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
    akinator_ui_set_definition();
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
                                            "Определение кого ты хочешь получить?")) != AKINATOR_SUCCESS) {
        free(way);
        return error_code;
    }
    if((error_code = akinator_print_definition(akinator, way, level)) != AKINATOR_SUCCESS) {
        free(way);
        return error_code;
    }

    free(way);
    AKINATOR_VERIFY(akinator);
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

    if(node->no == NULL && node->yes == NULL) {
        return AKINATOR_SUCCESS;
    }
    if(node->no == NULL || node->yes == NULL) {
        return AKINATOR_ONE_CHILD;
    }

    if(node->no->parent != node || node->yes->parent != node) {
        return AKINATOR_CHILD_PARENT_CONNECTION_ERROR;
    }

    RETURN_IF_ERROR(akinator_verify_children_parent(node->no));
    RETURN_IF_ERROR(akinator_verify_children_parent(node->yes));
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
                                                 MaxQuestionSize,
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
    _C_ASSERT(node != NULL, return AKINATOR_NODE_NULL);

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
    while(symbol != '{' && symbol != '}' && symbol != '\0' &&
          akinator->questions_storage_position < akinator->old_storage_size) {
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
                                                                   sizeof(new_container[0]));
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

    RETURN_IF_ERROR(akinator_print_message(akinator,
                                           "Это %s?",
                                           (*current_node)->question));

    akinator_answer_t answer = AKINATOR_ANSWER_UNKNOWN;
    RETURN_IF_ERROR(akinator_read_answer(&answer));

    switch(answer) {
        case AKINATOR_ANSWER_YES: {
            return akinator_handle_answer_yes(akinator, current_node);
        }
        case AKINATOR_ANSWER_NO: {
            return akinator_handle_answer_no(akinator, current_node);
        }
        case AKINATOR_ANSWER_UNKNOWN: {
            RETURN_IF_ERROR(akinator_print_message(akinator,
                                                   "Не понял ты чё не русский"));
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

    RETURN_IF_ERROR(akinator_get_answer_yes_no(answer));

    return AKINATOR_SUCCESS;
}

/*=============================================================================*/

akinator_error_t akinator_handle_answer_yes(akinator_t *akinator, akinator_node_t **current_node) {
    _C_ASSERT(current_node  != NULL, return AKINATOR_NODE_NULL);
    _C_ASSERT(*current_node != NULL, return AKINATOR_NODE_NULL);

    if(is_leaf(*current_node)) {
        RETURN_IF_ERROR(akinator_print_message(akinator, "Ну я же говорил"));
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
    RETURN_IF_ERROR(akinator_read_new_object_questions(akinator, current_node));
    RETURN_IF_ERROR(akinator_try_rewrite_database     (akinator));
    return AKINATOR_EXIT_SUCCESS;
}

/*=============================================================================*/

akinator_error_t akinator_read_new_object_questions(akinator_t *akinator, akinator_node_t *node) {
    _C_ASSERT(node != NULL, return AKINATOR_NODE_NULL);

    akinator_node_t *node_no  = node->no;
    akinator_node_t *node_yes = node->yes;

    RETURN_IF_ERROR(akinator_print_message(akinator,
                                           "Скажи ково или что ты загадал по братке."));
    RETURN_IF_ERROR(akinator_get_text_answer(node_yes->question));

    RETURN_IF_ERROR(akinator_print_message(akinator,
                                           "А что его отличает от %s`а?",
                                           node_no->question));
    RETURN_IF_ERROR(akinator_get_text_answer(node->question));
    return AKINATOR_SUCCESS;
}

/*=============================================================================*/

akinator_error_t akinator_try_rewrite_database(akinator_t *akinator) {
    AKINATOR_VERIFY(akinator);

    RETURN_IF_ERROR(akinator_print_message(akinator, "Ты хочешь чтобы я обновил свою базу данных? Риальна?"));
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
                RETURN_IF_ERROR(akinator_print_message(akinator, "Не понял ты чё не русский ШТОЛЕ???"));
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
                                              size_t           level) {
    _C_ASSERT(node     != NULL, return AKINATOR_NODE_NULL             );
    _C_ASSERT(database != NULL, return AKINATOR_DATABASE_OPENING_ERROR);

    fprintf(database, "%*s{\"%s\"", level*8, "", node->question);
    if(is_leaf(node)) {
        fputs("}\n", database);
        return AKINATOR_SUCCESS;
    }

    fputc('\n', database);

    RETURN_IF_ERROR(akinator_database_write_node(node->yes, database, level + 1));
    RETURN_IF_ERROR(akinator_database_write_node(node->no,  database, level + 1));

    fprintf("%*s}\n", level * 8, "", database);
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
    akinator_print_message(akinator, "%s", question);
    while(true) {
        char object[MaxQuestionSize] = {};
        RETURN_IF_ERROR(akinator_get_text_answer(object));


        akinator_error_t error_code = akinator_find_node(akinator, object, &node);
        if(error_code == AKINATOR_SUCCESS) {
            akinator_print_message(akinator, "Такова не знаю\n");
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

akinator_error_t akinator_print_definition(akinator_t *akinator,
                                           akinator_node_t **node_way,
                                           size_t            level) {
    _C_ASSERT(node_way != NULL, return AKINATOR_WAY_ARRAY_NULL    );
    _C_ASSERT(level    >= 1   , return AKINATOR_INVALID_NODE_LEVEL);

    akinator_definition_t definition = {};

    RETURN_IF_ERROR(akinator_add_definition(&definition,
                                            "%s - это ",
                                            node_way[0]->question));

    if(node_way[level - 1]->no == node_way[level - 2]) {
        RETURN_IF_ERROR(akinator_add_definition(&definition, "не "));
    }
    RETURN_IF_ERROR(akinator_add_definition(&definition,
                                            "%s, который ",
                                            node_way[level - 1]->question));

    for(size_t element = level - 2; element > 0; element--) {
        RETURN_IF_ERROR(akinator_print_definition_element(node_way,
                                                          element,
                                                          &definition));
    }
    akinator_print_message(akinator, definition.definition);
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

akinator_error_t akinator_print_difference(akinator_t       *akinator,
                                           size_t            level_first,
                                           akinator_node_t **way_first,
                                           size_t            level_second,
                                           akinator_node_t **way_second) {
    _C_ASSERT(way_first    != NULL, return AKINATOR_WAY_ARRAY_NULL    );
    _C_ASSERT(way_second   != NULL, return AKINATOR_WAY_ARRAY_NULL    );
    _C_ASSERT(level_first  >= 1   , return AKINATOR_INVALID_NODE_LEVEL);
    _C_ASSERT(level_second >= 1   , return AKINATOR_INVALID_NODE_LEVEL);

    akinator_definition_t definition = {};
    size_t index_first  = level_first  - 1;
    size_t index_second = level_second - 1;
    if(way_first[level_first - 2] == way_second[level_second - 2]) {
        RETURN_IF_ERROR(akinator_add_definition(&definition, "Они оба "));
    }
    while(way_first[index_first - 1] == way_second[index_second - 1] &&
          index_first  > 0 &&
          index_second > 0) {
        RETURN_IF_ERROR(akinator_print_definition_element(way_first,
                                                          index_first,
                                                          &definition));
        index_first--;
        index_second--;
    }

    if(way_first[level_first - 2] == way_second[level_second - 2]) {
        RETURN_IF_ERROR(akinator_add_definition(&definition, "но "));
    }
    RETURN_IF_ERROR(akinator_add_definition(&definition, "%s ",
                                            way_first[0]->question));
    for(; index_first > 0; index_first--) {
        RETURN_IF_ERROR(akinator_print_definition_element(way_first,
                                                          index_first,
                                                          &definition));
    }

    RETURN_IF_ERROR(akinator_add_definition(&definition, ", а %s ",
                                            way_second[0]->question));
    for(; index_second > 0; index_second--) {
        RETURN_IF_ERROR(akinator_print_definition_element(way_second,
                                                          index_second,
                                                          &definition));
    }

    akinator_print_message(akinator, definition.definition);
    fputc('\n', stdout);
    return AKINATOR_SUCCESS;
}

/*=============================================================================*/

akinator_error_t akinator_print_definition_element(akinator_node_t      **node_way,
                                                   size_t                 index,
                                                   akinator_definition_t *definition) {
    _C_ASSERT(node_way != NULL, return AKINATOR_WAY_ARRAY_NULL    );
    _C_ASSERT(index    >= 1   , return AKINATOR_INVALID_NODE_LEVEL);

    if(node_way[index]->no == node_way[index - 1]) {
        RETURN_IF_ERROR(akinator_add_definition(definition, "не "));
    }
    RETURN_IF_ERROR(akinator_add_definition(definition, "%s",
                                            node_way[index]->question));
    if(index != 1) {
        RETURN_IF_ERROR(akinator_add_definition(definition, ",\n"));
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
        size_t new_size = sizeof(akinator->leafs_array[0]) * akinator->leafs_array_capacity * 2;
        akinator_node_t **new_array = (akinator_node_t **)realloc(akinator->leafs_array,
                                                                  new_size);
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
                                                  akinator_node_t *parent) {
    _C_ASSERT(akinator != NULL, return AKINATOR_NULL_POINTER);
    _C_ASSERT(parent   != NULL, return AKINATOR_NODE_NULL   );

    RETURN_IF_ERROR(akinator_get_free_node(akinator, &parent->no));
    RETURN_IF_ERROR(akinator_get_free_node(akinator, &parent->yes));

    return AKINATOR_SUCCESS;
}

/*=============================================================================*/

akinator_error_t akinator_print_message(akinator_t *akinator,
                                        const char *format, ...) {
    va_list args;
    va_start(args, format);
    char string[MaxMessageSize + 1] = {};
    if((size_t)vsprintf(string, format, args) > MaxMessageSize) {
        color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                     "Message size is bigger than max, check constant in 'tts.h'\n");
        return AKINATOR_MESSAGE_SIZE_ERROR;
    }
    va_end(args);

    RETURN_IF_ERROR(akinator_ui_write_message(string));

    tts_speak(&akinator->tts, string);
    return AKINATOR_SUCCESS;
}

/*=============================================================================*/

akinator_error_t akinator_add_definition(akinator_definition_t *definition,
                                         const char            *format, ...) {
    va_list args;
    va_start(args, format);
    definition->index += vsprintf(definition->definition + definition->index, format, args);
    va_end(args);
    return AKINATOR_SUCCESS;
}
