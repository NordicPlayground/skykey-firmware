#include "display/lvgl_interface.h"
#include <zephyr.h>
#include "parse_util.h"


#include <logging/log.h>
LOG_MODULE_DECLARE(display_module, CONFIG_DISPLAY_MODULE_LOG_LEVEL);

///////////////////// VARIABLES ////////////////////
lv_obj_t * scr_welcome;
lv_obj_t * nordic_semi_logo;
lv_obj_t * nordic_semi_text;
lv_obj_t * label_welcome;

lv_obj_t *scr_fingerprint;
lv_obj_t *img_fingerprint;
lv_obj_t *label_fingerprint_instruction;
lv_obj_t *arc_register_progress;

lv_obj_t * scr_select_platform;
lv_obj_t * label_select_platform;
lv_obj_t * list_select_platform;
lv_group_t * list_group_select_platform;

lv_obj_t *scr_transmit;
lv_obj_t *img_bluetooth;
lv_obj_t *label_transmit_info;

lv_style_t * style_scr;
lv_style_t * style_list;

lv_obj_t* arc_timer;
lv_obj_t* bar_timer;
lv_obj_t* label_timer;

lv_color_t nordic_blue = LV_COLOR_MAKE(0x7f,0xd4,0xe6);

uint32_t timeout = CONFIG_CAF_POWER_MANAGER_TIMEOUT;
///////////////////// SIZE ////////////////////
#define DISP_WIDTH CONFIG_LVGL_HOR_RES_MAX
#define DISP_HEIGHT CONFIG_LVGL_VER_RES_MAX

///////////////////// IMAGES ////////////////////
#if CONFIG_LVGL_USE_IMG
LV_IMG_DECLARE(nordic_semi_w150px);   // assets\nordic_semi_w150px.png
LV_IMG_DECLARE(nordic_semi_text_w150px);   // assets\nordic_semi_text_w150px.png
LV_IMG_DECLARE(fingerprint_w100px); // assets/fingerprint_w100px.c???
LV_IMG_DECLARE(bluetooth_w60px);
#endif

///////////////////// FUNCTIONS ////////////////////
void change_screen(lv_obj_t * target)
{
    if (IS_ENABLED(CONFIG_LVGL_USE_ANIMATION)) {
        lv_scr_load_anim(target, LV_SCR_LOAD_ANIM_FADE_ON, CONFIG_DISPLAY_ANIMATION_TRANSITION_TIME_MSEC, 0, false);
    } else {
        lv_scr_load(target);
    }
}

//========================================================================================
/*                                                                                      *
 *                                  Component building                                  *
 *                                                                                      */
//========================================================================================

/**
 * Populates a list with entries.
 * @param opts Options to populate with
 * @param list list to be populated
*/
void populate_list(lv_obj_t *list, const char opts[CONFIG_DISPLAY_LIST_ENTRY_MAX_NUM][CONFIG_DISPLAY_LIST_ENTRY_MAX_LEN], const int num_opts)
{
    /* Reset list */
    lv_list_clean(list);
    lv_obj_set_size(list, DISP_WIDTH-20, DISP_HEIGHT-50);
    lv_obj_align(list, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, -10);

    static lv_style_t style_btn;
    lv_style_init(&style_btn);
    lv_style_set_bg_color(&style_btn, LV_BTN_STATE_DISABLED, nordic_blue);
    lv_style_set_value_color(&style_btn, LV_BTN_STATE_DISABLED, LV_COLOR_WHITE);
    /*Add buttons to the list*/
    lv_obj_t * list_btn;

    for (int i=0; i < num_opts; i++) {
        list_btn = lv_list_add_btn(list, NULL, opts[i]);
        lv_obj_add_style(list_btn, LV_BTN_PART_MAIN, &style_btn);
    }
}

/**
 * Initializes a timer (for the top right corner countdown)
*/

void initialize_timer_widget(void)
{
    label_timer = lv_label_create(scr_select_platform, NULL);
    lv_label_set_align(label_timer, LV_ALIGN_CENTER);
    lv_label_set_text(label_timer, "10");
    lv_obj_set_width(label_timer, 20);
    lv_obj_align(label_timer, scr_select_platform, LV_ALIGN_IN_TOP_RIGHT,-10, 10);

    static lv_style_t style_bar;
    lv_style_init(&style_bar);
    lv_style_set_bg_color(&style_bar, LV_STATE_DEFAULT, nordic_blue);

    static lv_style_t style_bar_bg;
    lv_style_init(&style_bar_bg);
    lv_style_set_bg_color(&style_bar_bg, LV_STATE_DEFAULT, LV_COLOR_WHITE);

    if (IS_ENABLED(CONFIG_LVGL_USE_BAR) && IS_ENABLED(CONFIG_CAF_POWER_MANAGER)) {
        bar_timer = lv_bar_create(scr_select_platform, NULL);
        lv_obj_set_size(bar_timer, DISP_WIDTH, 5);
        lv_obj_align_y(bar_timer, label_select_platform, LV_ALIGN_OUT_BOTTOM_MID, 9);
        lv_obj_align_x(bar_timer, scr_select_platform, LV_ALIGN_CENTER, 0);
        lv_bar_set_range(bar_timer, 0, timeout);
        lv_bar_set_value(bar_timer, 10, LV_ANIM_OFF);
        lv_obj_add_style(bar_timer, LV_BAR_PART_BG, &style_bar_bg);
        lv_obj_add_style(bar_timer, LV_BAR_PART_INDIC, &style_bar);
    }
}

void disp_set_timer(int remaining_time) 
{
    char remaining_time_str[4]; 
    sprintf(remaining_time_str, "%d", remaining_time);
    if (IS_ENABLED(CONFIG_LVGL_USE_BAR) && IS_ENABLED(CONFIG_CAF_POWER_MANAGER))
    {
        lv_bar_set_value(bar_timer, remaining_time, LV_ANIM_OFF);
        lv_label_set_text(label_timer, remaining_time_str);
    }
}

void disp_set_timeout_interval(int timeout_seconds) {
    timeout = timeout_seconds;
    if (IS_ENABLED(CONFIG_LVGL_USE_BAR) && IS_ENABLED(CONFIG_CAF_POWER_MANAGER)) {
        lv_bar_set_range(bar_timer, 0, timeout);
    }
}

///////////////////// SCREENS ////////////////////
void build_pages(void)
{
    /* Set background color to white */
	static lv_style_t style_screen;
	lv_style_init(&style_screen);
	lv_style_set_bg_color(&style_screen, LV_STATE_DEFAULT, LV_COLOR_WHITE);

    /* Init welcome screen */
    scr_welcome = lv_obj_create(NULL, NULL);
    lv_obj_add_style(scr_welcome, LV_OBJ_PART_MAIN, &style_screen);
    if (IS_ENABLED(CONFIG_LVGL_USE_IMG)) {
        nordic_semi_logo = lv_img_create(scr_welcome, NULL);
        lv_img_set_src(nordic_semi_logo, &nordic_semi_w150px);
        lv_obj_align(nordic_semi_logo, scr_welcome, LV_ALIGN_CENTER, 0, -20);
        nordic_semi_text = lv_img_create(scr_welcome, NULL);
        lv_img_set_src(nordic_semi_text, &nordic_semi_text_w150px);
        lv_obj_align(nordic_semi_text, nordic_semi_logo, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
    } else {
        label_welcome = lv_label_create(scr_welcome, NULL);
        lv_label_set_long_mode(label_welcome, LV_LABEL_LONG_BREAK);
        lv_label_set_align(label_welcome, LV_LABEL_ALIGN_CENTER);
        lv_label_set_text(label_welcome, "Welcome to Password Manager!");
        lv_obj_set_width(label_welcome, DISP_WIDTH - 20);
        lv_obj_align(label_welcome, NULL, LV_ALIGN_CENTER, 0, 0);
    }

    /* Init platform select screen */
    scr_select_platform = lv_obj_create(NULL, NULL);
    lv_obj_add_style(scr_select_platform, LV_OBJ_PART_MAIN, &style_screen);

    label_select_platform = lv_label_create(scr_select_platform, NULL);

    lv_label_set_long_mode(label_select_platform, LV_LABEL_LONG_EXPAND);
    lv_label_set_align(label_select_platform, LV_LABEL_ALIGN_LEFT);
    lv_label_set_text(label_select_platform, "No platforms found.");
    initialize_timer_widget();
    lv_obj_align(label_select_platform, scr_select_platform, LV_ALIGN_IN_TOP_LEFT, 10, 10);
    list_select_platform = lv_list_create(scr_select_platform, NULL);
    list_group_select_platform = lv_group_create();
    lv_group_add_obj(list_group_select_platform, list_select_platform);

    /* Init fingerprint register screen */
    scr_fingerprint = lv_obj_create(NULL, NULL);
    lv_obj_add_style(scr_fingerprint, LV_OBJ_PART_MAIN, &style_screen);

    if (IS_ENABLED(CONFIG_LVGL_USE_ARC)) {
        static lv_style_t style_arc_indic;
        lv_style_init(&style_arc_indic);
        lv_style_set_line_width(&style_arc_indic, LV_STATE_DEFAULT, 5);
        lv_style_set_line_color(&style_arc_indic, LV_STATE_DEFAULT, nordic_blue);

        static lv_style_t style_arc_bg;
        lv_style_init(&style_arc_bg);
        lv_style_set_border_color(&style_arc_bg, LV_STATE_DEFAULT, LV_COLOR_WHITE);
        lv_style_set_bg_color(&style_arc_bg, LV_STATE_DEFAULT, LV_COLOR_WHITE);
        lv_style_set_line_width(&style_arc_bg, LV_STATE_DEFAULT, 0);

        arc_register_progress = lv_arc_create(scr_fingerprint, NULL);
        lv_obj_add_style(arc_register_progress, LV_ARC_PART_INDIC, &style_arc_indic);
        lv_obj_add_style(arc_register_progress, LV_ARC_PART_BG, &style_arc_bg);
        lv_arc_set_bg_angles(arc_register_progress, 0, 360);
        lv_arc_set_rotation(arc_register_progress, 90);
        lv_arc_set_range(arc_register_progress, 0, 4);
        lv_arc_set_value(arc_register_progress, 0);

        lv_obj_set_size(arc_register_progress, 155,155);
        lv_obj_align(arc_register_progress, NULL, LV_ALIGN_CENTER, 0, 20);
    }
    if (IS_ENABLED(CONFIG_LVGL_USE_IMG))
    {
        img_fingerprint = lv_img_create(scr_fingerprint, NULL);
        lv_img_set_src(img_fingerprint, &fingerprint_w100px);
        lv_obj_align(img_fingerprint, scr_fingerprint, LV_ALIGN_CENTER, 0, 20);
    }

    label_fingerprint_instruction = lv_label_create(scr_fingerprint, NULL);
    lv_label_set_long_mode(label_fingerprint_instruction, LV_LABEL_LONG_BREAK);
    lv_label_set_align(label_fingerprint_instruction, LV_LABEL_ALIGN_CENTER);
    lv_label_set_text(label_fingerprint_instruction, "Plz register fingerprint");
    lv_obj_set_width(label_fingerprint_instruction, DISP_WIDTH - 20);
    lv_obj_align(label_fingerprint_instruction, NULL, LV_ALIGN_IN_TOP_MID, 0, 20);

    // TODO: Find replacement if image+arc are not enabled


    /* Init transmission screen */
    scr_transmit = lv_obj_create(NULL, NULL);
    lv_obj_add_style(scr_transmit, LV_OBJ_PART_MAIN, &style_screen);

    label_transmit_info = lv_label_create(scr_transmit, NULL);
    lv_label_set_align(label_transmit_info, LV_LABEL_ALIGN_CENTER);
    lv_label_set_text(label_transmit_info, "Inputting password...");
    lv_obj_align(label_transmit_info, scr_transmit, LV_ALIGN_CENTER, 0, 20);

    if (IS_ENABLED(CONFIG_LVGL_USE_IMG)) {
        img_bluetooth = lv_img_create(scr_transmit, NULL);
        lv_img_set_src(img_bluetooth, &bluetooth_w60px);
        lv_obj_align(img_bluetooth, scr_transmit, LV_ALIGN_CENTER, 0, 0);
        lv_obj_align(label_transmit_info, img_bluetooth, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    }
}



//========================================================================================
/*                                                                                      *
 *                                 Execution of commands                                *
 *                              called from display module                              *
 *                                                                                      */
//========================================================================================
void disp_widgets_init(void)
{
    build_pages();
    change_screen(scr_welcome);
}

void disp_widgets_clear(void)
{
    lv_obj_clean(scr_welcome);
    lv_obj_clean(scr_transmit);
    lv_obj_clean(scr_fingerprint);
    lv_obj_clean(scr_select_platform);
}

struct display_data disp_get_highlighted_option(void) 
{
    struct display_data info = {
        .id = DISPLAY_NO_DATA,
        .data = ""};
    char* chosen_label = lv_label_get_text(lv_list_get_btn_label(lv_list_get_btn_selected(list_select_platform)));
    if (chosen_label != NULL) {
        strncpy(info.data, chosen_label, CONFIG_DISPLAY_LIST_ENTRY_MAX_LEN);
        info.id = DISPLAY_PLATFORM_CHOSEN;
    }
    return info;
}

void disp_set_platform_list_contents(const char *platform_names)
{
    lv_label_set_text(label_select_platform, "Platform select");

    char opts[CONFIG_DISPLAY_LIST_ENTRY_MAX_NUM][CONFIG_DISPLAY_LIST_ENTRY_MAX_LEN];
    int num_opts = 0;
    char *rest = (char *)platform_names;
    char *token = strtok_r(rest, "\t", &rest);
    while (token != NULL)
    {

        strncpy(opts[num_opts], token, CONFIG_DISPLAY_LIST_ENTRY_MAX_LEN);
        num_opts++;
        if (num_opts > CONFIG_DISPLAY_LIST_ENTRY_MAX_NUM)
        {
            LOG_ERR("Number of platform options exceed the maximum configurated value. Consider increasing CONFIG_DISPLAY_LIST_ENTRY_MAX_NUM.");
            break;
        }
        token = strtok_r(NULL, "\t", &rest);
    }
    // Populating the list inside the loop causes a buffer overflow.
    populate_list(list_select_platform, opts, num_opts);
}

void disp_set_screen(enum scr_state_type to_screen_state)
{
    switch (to_screen_state)
    {
    case SCR_STATE_WELCOME:
        change_screen(scr_welcome);
        return;
    case SCR_STATE_FINGERPRINT:
        change_screen(scr_fingerprint);
        return;
    case SCR_STATE_SELECT_PLATFORM:
        change_screen(scr_select_platform);
        return;
    case SCR_STATE_TRANSMIT:
        change_screen(scr_transmit);
        return;
    default:
        return;
    }
}

void disp_scroll_platform_list(bool up)
{
    if (up)
    {
        lv_group_send_data(list_group_select_platform, LV_KEY_UP);
    }
    else
    {
        lv_group_send_data(list_group_select_platform, LV_KEY_DOWN);
    }
}

void disp_set_fingerprint_progress(int enroll_number) {
    lv_arc_set_value(arc_register_progress, enroll_number);
}