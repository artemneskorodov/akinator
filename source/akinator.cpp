#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>

#include "akinator.h"
#include "text_buffer.h"
#include "colors.h"

enum akinator_answer_t {
    AKINATOR_ANSWER_YES,
    AKINATOR_ANSWER_NO,
    AKINATOR_ANSWER_UNKNOWN,
};

static const size_t max_question_size = 64;
static const size_t text_container_size = 64;
static const size_t akinator_container_size = 64;
static const size_t max_system_command_length = 256;
static const size_t max_filename_size = 64;

static const char *general_dump_filename = "logs/akin.html";
static const char *logs_folder = "logs";
static const char *dot_utf8_folder = "dot_utf8";
static const char *dot_cp1251_folder = "dot_cp1251";
static const char *img_folder = "img";

static akinator_error_t akinator_read_database(akinator_t *akinator, FILE *database);
static akinator_error_t akinator_database_read_node(akinator_t *akinator, akinator_node_t *node, FILE *database);
static akinator_error_t akinator_clean_buffer(FILE *database);
static akinator_error_t akinator_get_free_node(akinator_t *akinator, akinator_node_t **node);
static akinator_error_t akinator_ask_question(akinator_t *akinator, akinator_node_t **current_node);
static akinator_error_t akinator_read_answer(akinator_answer_t *answer);
static akinator_error_t akinator_handle_answer_yes(akinator_node_t **current_node);
static akinator_error_t akinator_handle_answer_no(akinator_t *akinator, akinator_node_t **current_node);
static akinator_error_t akinator_handle_new_object(akinator_t *akinator, akinator_node_t **current_node);
static bool is_leaf(akinator_node_t *node);
static akinator_error_t akinator_update_database(akinator_t *akinator);
static akinator_error_t akinator_database_write_node(akinator_node_t *node, FILE *database);
static akinator_error_t akinator_dump_init(akinator_t *akinator);
static akinator_error_t akinator_create_dot_cp1251_dump(const char *filename_dot_cp1251, akinator_t *akinator);
static akinator_error_t akinator_create_img_dump(const char *filename_dot_cp1251, const char *filename_dot_utf8, const char *filename_img);
static akinator_error_t akinator_add_general_dump(const char *filename_img, akinator_t *akinator);
static akinator_error_t akinator_dot_dump_write_header(FILE *dot_file, akinator_t *akinator);
static akinator_error_t akinator_dump_node(akinator_t *akinator, akinator_node_t *node, FILE *dot_file, size_t level);
static akinator_error_t akinator_dot_dump_write_footer(FILE *dot_file);
static akinator_error_t akinator_get_node_color(akinator_t *akinator, akinator_node_t *node, const char **color);
static akinator_error_t akinator_run_system_command(const char *format, ...);
static akinator_error_t akinator_init_new_object_children(akinator_t *akinator, akinator_node_t **current_node);
static akinator_error_t akinator_database_read_children(akinator_t *akinator, akinator_node_t *node, FILE *database);

akinator_error_t akinator_ctor(akinator_t *akinator, const char *database_filename) {
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);
    FILE *database = fopen(database_filename, "r");
    if(database == NULL) {
        color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                     "Error while opening database.\n");
        return AKINATOR_DATABASE_OPENING_ERROR;
    }

    akinator->container_size = akinator_container_size;
    akinator->database_name = database_filename;

    akinator_error_t error_code = AKINATOR_SUCCESS;
    if((error_code = akinator_read_database(akinator,
                                            database)) != AKINATOR_SUCCESS) {
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
    text_buffer_dtor(&akinator->questions_storage);

    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_dump(akinator_t *akinator) {
    char filename_dot_cp1251[max_filename_size] = {};
    char filename_dot_utf8  [max_filename_size] = {};
    char filename_img       [max_filename_size] = {};

    sprintf(filename_dot_cp1251,
            "%s\\%s\\dump%08llx.dot",
            logs_folder,
            dot_cp1251_folder,
            akinator->dumps_number);
    sprintf(filename_dot_utf8,
            "%s\\%s\\dump%08llx.dot",
            logs_folder,
            dot_utf8_folder,
            akinator->dumps_number);
    sprintf(filename_img,
            "%s\\dump%08llx.svg",
            img_folder,
            akinator->dumps_number);

    akinator_error_t error_code = AKINATOR_SUCCESS;
    if((error_code = akinator_create_dot_cp1251_dump(filename_dot_cp1251,
                                                     akinator)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    if((error_code = akinator_create_img_dump(filename_dot_cp1251,
                                              filename_dot_utf8,
                                              filename_img)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    if((error_code = akinator_add_general_dump(filename_img,
                                               akinator)) != AKINATOR_SUCCESS) {
        return error_code;
    }

    akinator->dumps_number++;
    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_create_dot_cp1251_dump(const char *filename_dot_cp1251,
                                                 akinator_t *akinator) {
    FILE *dot_file = fopen(filename_dot_cp1251, "w");
    if(dot_file == NULL) {
        color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                     "Error while opening dot file.\n");
        return AKINATOR_DOT_FILE_OPENING_ERROR;
    }

    akinator_error_t error_code = AKINATOR_SUCCESS;
    if((error_code = akinator_dot_dump_write_header(dot_file,
                                                    akinator)) != AKINATOR_SUCCESS) {
        fclose(dot_file);
        return error_code;
    }
    if((error_code = akinator_dump_node(akinator,
                                        akinator->root,
                                        dot_file, 0)) != AKINATOR_SUCCESS) {
        fclose(dot_file);
        return error_code;
    }
    if((error_code = akinator_dot_dump_write_footer(dot_file)) != AKINATOR_SUCCESS) {
        fclose(dot_file);
        return error_code;
    }

    fclose(dot_file);
    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_dot_dump_write_header(FILE      *dot_file,
                                               akinator_t *akinator) {
    if(fputs("digraph {\n"
             "node[shape = Mrecord, style = filled];\n"
             "rankdir = TB;\n",
             dot_file) < 0) {
        color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                     "Error while writing dump.\n");
        return AKINATOR_WRITING_DUMP_ERROR;
    }

    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_dump_node(akinator_t      *akinator,
                                    akinator_node_t *node,
                                    FILE            *dot_file,
                                    size_t           level) {
    akinator_error_t error_code = AKINATOR_SUCCESS;
    const char *color = NULL;
    if((error_code = akinator_get_node_color(akinator,
                                             node,
                                             &color)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    fprintf(dot_file,
            "node%p[rank = %llu, "
            "label = \"{ %s | { <yes> ДА | <no> НЕТ } }\", "
            "color = \"%s\"];\n",
            node,
            level,
            node->question,
            color);
    if(is_leaf(node)) {
        return AKINATOR_SUCCESS;
    }

    fprintf(dot_file,
            "node%p -> node%p\n",
            node,
            node->yes);
    fprintf(dot_file,
            "node%p -> node%p\n",
            node,
            node->no );

    if((error_code = akinator_dump_node(akinator,
                                        node->yes,
                                        dot_file,
                                        level + 1)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    if((error_code = akinator_dump_node(akinator,
                                        node->no,
                                        dot_file,
                                        level + 1)) != AKINATOR_SUCCESS) {
        return error_code;
    }

    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_get_node_color(akinator_t *akinator, akinator_node_t *node, const char **color) {
    *color = "#ffffff";
    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_dot_dump_write_footer(FILE *dot_file) {
    if(fputs("}\n", dot_file) < 0) {
        color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                     "Error while writing dump.\n");
        return AKINATOR_WRITING_DUMP_ERROR;
    }
    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_create_img_dump(const char *filename_dot_cp1251,
                                          const char *filename_dot_utf8,
                                          const char *filename_img) {
    akinator_error_t error_code = AKINATOR_SUCCESS;
    if((error_code = akinator_run_system_command("powershell -command \"Get-Content %s | "
                                                 "Set-Content -Encoding utf8 %s\"",
                                                 filename_dot_cp1251,
                                                 filename_dot_utf8)) != AKINATOR_SUCCESS) {
        return error_code;
    }

    FILE *utf8_dot_file = fopen(filename_dot_utf8, "r+");
    if(utf8_dot_file == NULL) {
        color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                     "Error while opening utf8 dot file.\n");
        return AKINATOR_DOT_FILE_OPENING_ERROR;
    }
    for(size_t symbol = 0; symbol < 3; symbol++) {
        fputc(' ', utf8_dot_file);
    }
    fclose(utf8_dot_file);

    if((error_code = akinator_run_system_command("dot %s -Tsvg -o %s\\%s",
                                                 filename_dot_utf8,
                                                 logs_folder,
                                                 filename_img)) != AKINATOR_SUCCESS) {
        return error_code;
    }

    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_add_general_dump(const char *filename_img,
                                           akinator_t *akinator) {
    fprintf(akinator->general_dump,
            "DUMP %llu\n"
            "<img src = \"%s\">\n",
            akinator->dumps_number,
            filename_img);
    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_read_database(akinator_t *akinator, FILE *database) {
    akinator_error_t error_code = AKINATOR_SUCCESS;
    if((error_code = text_buffer_ctor(&akinator->questions_storage,
                                      max_question_size,
                                      text_container_size)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    if((error_code = akinator_get_free_node(akinator,
                                            &akinator->root)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    if((error_code = akinator_database_read_node(akinator,
                                                 akinator->root,
                                                 database)) != AKINATOR_SUCCESS) {
        return error_code;
    }

    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_database_read_node(akinator_t      *akinator,
                                             akinator_node_t *node,
                                             FILE            *database) {
    if(fgetc(database) != '{') {
        color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                     "Error while reading database.\n");
        return AKINATOR_DATABASE_READING_ERROR;
    }

    akinator_error_t error_code = AKINATOR_SUCCESS;
    if((error_code = text_buffer_add(&akinator->questions_storage,
                                     &node->question)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    if(fscanf(database, "\"%[^\"]\"", node->question) < 0) {
        color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                     "Error while reading database.\n");
        return AKINATOR_DATABASE_READING_ERROR;
    }
    if(fgetc(database) == '}') {
        if((error_code = akinator_clean_buffer(database)) != AKINATOR_SUCCESS) {
            return error_code;
        }
        return AKINATOR_SUCCESS;
    }

    if((error_code = akinator_database_read_children(akinator,
                                                     node,
                                                     database)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    if(fgetc(database) != '}') {
        color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                     "Error while reading database.\n");
        return AKINATOR_DATABASE_READING_ERROR;
    }
    if((error_code = akinator_clean_buffer(database)) != AKINATOR_SUCCESS) {
        return error_code;
    }

    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_database_read_children(akinator_t      *akinator,
                                                 akinator_node_t *node,
                                                 FILE            *database) {
    akinator_error_t error_code = AKINATOR_SUCCESS;

    if((error_code = akinator_get_free_node(akinator,
                                            &node->no)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    if((error_code = akinator_get_free_node(akinator,
                                            &node->yes)) != AKINATOR_SUCCESS) {
        return error_code;
    }

    if((error_code = akinator_clean_buffer(database)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    if((error_code = akinator_database_read_node(akinator,
                                                 node->yes,
                                                 database)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    if((error_code = akinator_database_read_node(akinator,
                                                 node->no,
                                                 database)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    if((error_code = akinator_clean_buffer(database)) != AKINATOR_SUCCESS) {
        return error_code;
    }

    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_clean_buffer(FILE *database) {
    int symbol = 0;
    while(!isgraph(symbol) && symbol != EOF) {
        symbol = fgetc(database);
    }
    fseek(database, -1, SEEK_CUR);

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
    akinator_node_t *node_no = (*current_node)->no;
    akinator_node_t *node_yes = (*current_node)->yes;

    color_printf(MAGENTA_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                 "Скажи ково или что ты загадал по братке.\n");
    fgetc(stdin);
    scanf("%[^\n]", node_yes->question);

    color_printf(MAGENTA_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                 "А что его отличает от %sа?\n",
                 node_no->question);
    fgetc(stdin);
    scanf("%[^\n]", (*current_node)->question);

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
    return AKINATOR_EXIT_SUCCESS;
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
    if((error_code = text_buffer_add(&akinator->questions_storage,
                                     &(*current_node)->question)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    if((error_code = text_buffer_add(&akinator->questions_storage,
                                     &(*current_node)->yes->question)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    return AKINATOR_SUCCESS;
}

bool is_leaf(akinator_node_t *node) {
    if(node->no == NULL) {
        return true;
    }
    else {
        return false;
    }
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
                                                  database)) != AKINATOR_SUCCESS) {
        return error_code;
    }

    return AKINATOR_EXIT_SUCCESS;
}

akinator_error_t akinator_database_write_node(akinator_node_t *node,
                                              FILE            *database) {
    fprintf(database, "{\"%s\"", node->question);
    if(is_leaf(node)) {
        fputs("}\n", database);
        return AKINATOR_SUCCESS;
    }

    fputc('\n', database);

    akinator_error_t error_code = AKINATOR_SUCCESS;
    if((error_code = akinator_database_write_node(node->yes,
                                                  database)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    if((error_code = akinator_database_write_node(node->no,
                                                  database)) != AKINATOR_SUCCESS) {
        return error_code;
    }

    fputs("}\n", database);
    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_dump_init(akinator_t *akinator) {
    akinator_error_t error_code = AKINATOR_SUCCESS;
    if((error_code = akinator_run_system_command("md %s",
                                                 logs_folder)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    if((error_code = akinator_run_system_command("md %s\\%s",
                                                 logs_folder,
                                                 dot_utf8_folder)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    if((error_code = akinator_run_system_command("md %s\\%s",
                                                 logs_folder,
                                                 dot_cp1251_folder)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    if((error_code = akinator_run_system_command("md %s\\%s",
                                                 logs_folder,
                                                 img_folder)) != AKINATOR_SUCCESS) {
        return error_code;
    }

    akinator->general_dump = fopen(general_dump_filename, "w");
    if(akinator->general_dump == NULL) {
        color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                     "Error while opening akinator dump file.");
        return AKINATOR_GENERAL_DUMP_OPENING_ERROR;
    }

    fputs("<pre>", akinator->general_dump);
    return AKINATOR_SUCCESS;
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
