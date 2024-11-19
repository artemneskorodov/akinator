#include "TXLib.h"
#include "graphics.h"
#include "colors.h"

struct point_t {
    double x;
    double y;
};

struct rectangle_t {
    double left;
    double right;
    double bottom;
    double top;
};

struct akinator_menu_t {
    rectangle_t *button_boxes;
    const char **labels;
    size_t number;
};

static HDC MainMenuBackground       = NULL;
static HDC GuessPageBackground      = NULL;
static HDC DefinitionPageBackground = NULL;
static HDC DifferencePageBackground = NULL;

static const size_t  ScreenWidth           = 800;
static const size_t  ScreenHeight          = 600;
static const size_t  MainMenuButtonsNumber = 4;
static const double  MainMenuButtonsWidth  = 300;
static const double  MainMenuButtonsHeight = 50;
static const double  YesNoButtonsWidth     = 200;
static const double  YesNoButtonsHeight    = 50;
static const point_t YesNoButtonsCenter    = {.x = ScreenWidth  - YesNoButtonsWidth,
                                              .y = ScreenHeight - YesNoButtonsHeight * 2};

static const COLORREF TextColor           = RGB(0xff, 0xff, 0xff);
static const COLORREF BoxColor            = RGB(0x00, 0x00, 0x00);
static const COLORREF HighlightedBoxColor = RGB(0x21, 0x12, 0x11);

static const rectangle_t DefaultMessageBox = {.left = 10, .right = 500, .bottom = 20, .top = 300};

static double           get_main_menu_box_center_y( size_t number);

static akinator_error_t akinator_run_menu          (akinator_menu_t *menu,
                                                    size_t          *chosen_case);

static akinator_error_t akinator_draw_menu         (akinator_menu_t *menu,
                                                    size_t           highlighted);

static bool             is_in                      (point_t *point, rectangle_t *rect);

akinator_error_t akinator_graphics_init(void) {
    txCreateWindow(ScreenWidth, ScreenHeight);
    MainMenuBackground = txLoadImage("img/main_menu.bmp");
    if(MainMenuBackground == NULL) {
        // color_printf(RED_TEXT, BOLD_TEXT, DEFAULT_BACKGROUND,
        //              "Error while loading main menu image.\n");
        return AKINATOR_IMAGE_LOADING_ERROR;
    }

    GuessPageBackground = txLoadImage("img/main_menu.bmp");
    DefinitionPageBackground = txLoadImage("img/main_menu.bmp");
    DifferencePageBackground = txLoadImage("img/main_menu.bmp");

    txBitBlt(txDC(), 0, 0, ScreenWidth, ScreenHeight, MainMenuBackground);
    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_graphics_dtor(void) {
    txDeleteDC(MainMenuBackground);
    txDeleteDC(GuessPageBackground);
    txDeleteDC(DefinitionPageBackground);
    txDeleteDC(DifferencePageBackground);

    MainMenuBackground = NULL;
    GuessPageBackground = NULL;
    DefinitionPageBackground = NULL;
    DifferencePageBackground = NULL;
    txDisableAutoPause();
    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_go_to_main_menu(main_menu_cases_t *output) {
    txBitBlt(txDC(), 0, 0, ScreenWidth, ScreenHeight, MainMenuBackground);

    const char *labels[] = {"Давай рескни",
                            "Определение",
                            "Разница",
                            "Выход"};
    rectangle_t rectangles[MainMenuButtonsNumber] = {};
    for(size_t i = 0; i < MainMenuButtonsNumber; i++) {
        double box_center_y  = get_main_menu_box_center_y(i);

        rectangles[i].left   = (ScreenWidth - MainMenuButtonsWidth) / 2;
        rectangles[i].right  = (ScreenWidth + MainMenuButtonsWidth) / 2;
        rectangles[i].top    = box_center_y + MainMenuButtonsHeight / 2;
        rectangles[i].bottom = box_center_y - MainMenuButtonsHeight / 2;
    }

    akinator_menu_t menu = {
        .button_boxes = rectangles,
        .labels       = labels,
        .number       = MainMenuButtonsNumber
    };

    akinator_run_menu(&menu, (size_t *)output);

    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_run_menu(akinator_menu_t *menu, size_t *chosen_case) {
    txBegin();
    akinator_draw_menu(menu, *chosen_case);
    while(true) {
        POINT   tx_mouse_pos   = txMousePos();
        point_t mouse_position = {.x = (double)tx_mouse_pos.x,
                                  .y = (double)tx_mouse_pos.y};
        bool is_set     = false;
        bool is_running = true;
        for(size_t i = 0; i < menu->number; i++) {
            if(is_in(&mouse_position, menu->button_boxes + i)) {
                *chosen_case = i;
                is_set = true;
                if(txGetAsyncKeyState(VK_LBUTTON)) {
                    is_running = false;
                }
            }
        }
        if(!is_set) {
            *chosen_case = menu->number;
        }
        if(!is_running) {
            akinator_draw_menu(menu, menu->number);
            break;
        }
        akinator_draw_menu(menu, *chosen_case);
    }
    txEnd();
    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_draw_menu(akinator_menu_t *menu, size_t highlighted) {
    for(size_t i = 0; i < menu->number; i++) {
        COLORREF fill_color = BoxColor;
        COLORREF color      = BoxColor;
        if(i == highlighted) {
            fill_color = HighlightedBoxColor;
            color = HighlightedBoxColor;
        }

        txSetColor(color);
        txSetFillColor(fill_color);
        rectangle_t *box_rectangle = menu->button_boxes + i;
        txRectangle(box_rectangle->left,
                    box_rectangle->bottom,
                    box_rectangle->right,
                    box_rectangle->top);

        txSetColor(TextColor);
        txDrawText(box_rectangle->left,
                   box_rectangle->bottom,
                   box_rectangle->right,
                   box_rectangle->top,
                   menu->labels[i]);
    }
    txRedrawWindow();

    return AKINATOR_SUCCESS;
}

bool is_in(point_t *point, rectangle_t *rect) {
    if(point->x < rect->left ||
       point->x > rect->right) {
        return false;
    }
    if(point->y < rect->bottom ||
       point->y > rect->top) {
        return false;
    }

    return true;
}

double get_main_menu_box_center_y(size_t number) {
    return ScreenHeight / 2 + (2 * (double)number - (double)MainMenuButtonsNumber) * MainMenuButtonsHeight;
}

akinator_error_t akinator_ui_set_guess(void) {
    txBitBlt(txDC(), 0, 0, ScreenWidth, ScreenHeight, GuessPageBackground);

    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_ui_set_definition(void) {
    txBitBlt(txDC(), 0, 0, ScreenWidth, ScreenHeight, DefinitionPageBackground);

    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_ui_set_difference(void) {
    txBitBlt(txDC(), 0, 0, ScreenWidth, ScreenHeight, DifferencePageBackground);

    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_ui_write_message(const char *message) {
    txSetColor(BoxColor);
    txSetFillColor(BoxColor);
    txRectangle(DefaultMessageBox.left,
                DefaultMessageBox.bottom,
                DefaultMessageBox.right,
                DefaultMessageBox.top);

    txSetColor(TextColor);
    txSetTextAlign(TA_CENTER);
    txDrawText(DefaultMessageBox.left,
               DefaultMessageBox.bottom,
               DefaultMessageBox.right,
               DefaultMessageBox.top, message);

    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_get_answer_yes_no(akinator_answer_t *answer) {
    const char *labels[] = {"да", "нет"};
    rectangle_t rectangles[2] = {};
    for(size_t i = 0; i < 2; i++) {
        rectangles[i].left   = YesNoButtonsCenter.x - YesNoButtonsWidth / 2;
        rectangles[i].right  = YesNoButtonsCenter.x + YesNoButtonsWidth / 2;
        rectangles[i].top    = YesNoButtonsCenter.y + 1.5 * (double)i * YesNoButtonsHeight + YesNoButtonsHeight / 2;
        rectangles[i].bottom = YesNoButtonsCenter.y + 1.5 * (double)i * YesNoButtonsHeight - YesNoButtonsHeight / 2;
    }

    akinator_menu_t menu = {
        .button_boxes = rectangles,
        .labels = labels,
        .number = 2
    };

    size_t chosen = 0;
    akinator_run_menu(&menu, &chosen);
    if(chosen == 0) {
        *answer = AKINATOR_ANSWER_YES;
    }
    else if(chosen == 1) {
        *answer = AKINATOR_ANSWER_NO;
    }
    else {
        *answer = AKINATOR_ANSWER_UNKNOWN;
    }

    return AKINATOR_SUCCESS;
}

akinator_error_t akinator_get_text_answer(char *output) {
    const char *input = txInputBox("чё надо");
    strcpy(output, input);
    return AKINATOR_SUCCESS;
}
