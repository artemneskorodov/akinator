#include "colors.h"
#include "akinator_dump.h"
#include "akinator_utils.h"

static const size_t      max_filename_size     = 128;
static const char *const general_dump_filename = "logs/akin.html";
static const char *const logs_folder           = "logs";
static const char *const dot_utf8_folder       = "dot_utf8";
static const char *const dot_cp1251_folder     = "dot_cp1251";
static const char *const img_folder            = "img";
static const char *const root_color            = "#b8b8ff";
static const char *const node_color            = "#ffeedd";
static const char *const leaf_color            = "#ffd8be";
static const char *const dump_background       = "#ced4da";

struct dump_filenames_t {
    char dot_cp1251[max_filename_size] = {};
    char dot_utf8  [max_filename_size] = {};
    char img       [max_filename_size] = {};
};

static akinator_error_t akinator_create_dot_cp1251_dump (dump_filenames_t *filenames,
                                                         akinator_t       *akinator);

static akinator_error_t akinator_create_img_dump        (dump_filenames_t *filenames);

static akinator_error_t akinator_add_general_dump       (const char       *filename_img,
                                                         akinator_t       *akinator,
                                                         const char       *caller_file,
                                                         size_t            caller_line,
                                                         const char       *caller_func);

static akinator_error_t akinator_dot_dump_write_header  (FILE             *dot_file);

static akinator_error_t akinator_dump_node              (akinator_t       *akinator,
                                                         akinator_node_t  *node,
                                                         FILE             *dot_file,
                                                         size_t            level);

static akinator_error_t akinator_dot_dump_write_footer  (FILE             *dot_file);

static akinator_error_t akinator_get_node_color         (akinator_t       *akinator,
                                                         akinator_node_t  *node,
                                                         const char      **color);

static akinator_error_t akinator_dump_filenames_init    (akinator_t       *akinator,
                                                         dump_filenames_t *filenames);

akinator_error_t akinator_dump(akinator_t *akinator,
                               const char *caller_file,
                               size_t      caller_line,
                               const char *caller_func) {
    dump_filenames_t filenames = {};
    akinator_error_t error_code = AKINATOR_SUCCESS;

    if((error_code = akinator_dump_filenames_init(akinator,
                                                  &filenames)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    if((error_code = akinator_create_dot_cp1251_dump(&filenames,
                                                     akinator)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    if((error_code = akinator_create_img_dump(&filenames)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    if((error_code = akinator_add_general_dump(filenames.img,
                                               akinator,
                                               caller_file,
                                               caller_line,
                                               caller_func)) != AKINATOR_SUCCESS) {
        return error_code;
    }

    akinator->dumps_number++;
    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_create_dot_cp1251_dump(dump_filenames_t *filenames,
                                                 akinator_t       *akinator) {
    FILE *dot_file = fopen(filenames->dot_cp1251, "w");
    if(dot_file == NULL) {
        color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
                     "Error while opening dot file.\n");
        return AKINATOR_DOT_FILE_OPENING_ERROR;
    }

    akinator_error_t error_code = AKINATOR_SUCCESS;
    if((error_code = akinator_dot_dump_write_header(dot_file)) != AKINATOR_SUCCESS) {
        fclose(dot_file);
        return error_code;
    }
    if(fputs("subgraph tree_dump{\n", dot_file) < 0) {
        fclose(dot_file);
        dot_file = NULL;
        return AKINATOR_WRITING_DUMP_ERROR;
    }
    if((error_code = akinator_dump_node(akinator,
                                        akinator->root,
                                        dot_file, 0)) != AKINATOR_SUCCESS) {
        fclose(dot_file);
        dot_file = NULL;
        return error_code;
    }

    if(fputs("}\n", dot_file) < 0) {
        fclose(dot_file);
        dot_file = NULL;
        return AKINATOR_WRITING_DUMP_ERROR;
    }
    if((error_code = akinator_dot_dump_write_footer(dot_file)) != AKINATOR_SUCCESS) {
        fclose(dot_file);
        return error_code;
    }

    if(fclose(dot_file) != 0) {
        return AKINATOR_WRITING_DUMP_ERROR;
    }
    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_dot_dump_write_header(FILE *dot_file) {
    if(fprintf(dot_file,
               "digraph {\n"
               "bgcolor = \"%s\";\n"
               "node[shape = Mrecord, style = filled];\n"
               "rankdir = TB;\n",
               dump_background) < 0) {
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
    if(fprintf(dot_file,
               "node%p[rank = %llu, "
               "label = \"{ %s | { <yes> �� | <no> ��� } }\", "
               "fillcolor = \"%s\"];\n",
               node,
               level,
               node->question,
               color) < 0) {
        return AKINATOR_WRITING_DUMP_ERROR;
    }
    if(is_leaf(node)) {
        return AKINATOR_SUCCESS;
    }

    if(fprintf(dot_file,
               "node%p -> node%p\n"
               "node%p -> node%p\n",
               node,
               node->yes,
               node,
               node->no) < 0) {
        return AKINATOR_WRITING_DUMP_ERROR;
    }

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
    if(akinator->root == node) {
        *color = root_color;
    }
    else if(is_leaf(node)) {
        *color = leaf_color;
    }
    else {
        *color = node_color;
    }
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

akinator_error_t akinator_create_img_dump(dump_filenames_t *filenames) {
    akinator_error_t error_code = AKINATOR_SUCCESS;
    if((error_code = akinator_run_system_command("powershell -command \"Get-Content %s | "
                                                 "Set-Content -Encoding utf8 %s\"",
                                                 filenames->dot_cp1251,
                                                 filenames->dot_utf8)) != AKINATOR_SUCCESS) {
        return error_code;
    }
    FILE *utf8_dot_file = fopen(filenames->dot_utf8, "r+");
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
                                                 filenames->dot_utf8,
                                                 logs_folder,
                                                 filenames->img)) != AKINATOR_SUCCESS) {
        return error_code;
    }

    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_add_general_dump(const char *filename_img,
                                           akinator_t *akinator,
                                           const char *caller_file,
                                           size_t      caller_line,
                                           const char *caller_func) {
    fprintf(akinator->general_dump,
            "<div style = \"background: %s; border-radius: 25px; padding: 10px; margin: 10px;\">"
            "<h1>DUMP %llu</h1>"
            "<h2>called from %s:%llu, caller function: %s</h2>"
            "<img src = \"%s\">\n"
            "</div>\n",
            dump_background,
            akinator->dumps_number,
            caller_file,
            caller_line,
            caller_func,
            filename_img);

    fflush(akinator->general_dump);
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

    const char *legend_element_styles = "border-radius:12px;"
                                        "height:25px;"
                                        "width:120px;"
                                        "border:solid;"
                                        "text-align:center;";

    fprintf(akinator->general_dump,
            "<pre>\n"
            "<div style = \"background: %s; border-radius: 25px; padding: 10px; margin: 10px;\">"
            "<h2 style = \"background:%s;%s\">root color</h2>"
            "<h2 style = \"background:%s;%s\">leaf color</h2>"
            "<h2 style = \"background:%s;%s\">node color</h2>"
            "</div>",
            dump_background,
            root_color, legend_element_styles,
            leaf_color, legend_element_styles,
            node_color, legend_element_styles);
    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_dump_filenames_init(akinator_t       *akinator,
                                              dump_filenames_t *filenames) {
    sprintf(filenames->dot_cp1251,
            "%s\\%s\\dump%08llx.dot",
            logs_folder,
            dot_cp1251_folder,
            akinator->dumps_number);
    sprintf(filenames->dot_utf8,
            "%s\\%s\\dump%08llx.dot",
            logs_folder,
            dot_utf8_folder,
            akinator->dumps_number);
    sprintf(filenames->img,
            "%s\\dump%08llx.svg",
            img_folder,
            akinator->dumps_number);
    return AKINATOR_SUCCESS;
}
