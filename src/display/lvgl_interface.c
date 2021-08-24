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
lv_obj_t *label_instruction;
lv_obj_t *arc_register_progress;

lv_obj_t * scr_select_platform;
lv_obj_t * label_select_platform;
lv_obj_t * list_select_platform;
lv_group_t * list_group_select_platform;

lv_obj_t *scr_transmit;
lv_obj_t *label_transmit_info;

lv_style_t * style_scr;
lv_style_t * style_list;

lv_color_t nordic_blue = LV_COLOR_MAKE(0x7f,0xd4,0xe6);

///////////////////// SIZE ////////////////////
#define DISP_WIDTH CONFIG_LVGL_HOR_RES_MAX
#define DISP_HEIGHT CONFIG_LVGL_VER_RES_MAX

///////////////////// IMAGES ////////////////////
#if CONFIG_LVGL_USE_IMG
LV_IMG_DECLARE(nordic_semi_w150px);   // assets\nordic_semi_w150px.png
LV_IMG_DECLARE(nordic_semi_text_w150px);   // assets\nordic_semi_text_w150px.png
#endif

///////////////////// FUNCTIONS ////////////////////
void change_screen(lv_obj_t * target)
{
    
#if CONFIG_LVGL_USE_ANIMATION
    lv_scr_load_anim(target, LV_SCR_LOAD_ANIM_FADE_ON, CONFIG_DISPLAY_ANIMATION_TRANSITION_TIME_MSEC, 0, false);
#else
    lv_scr_load(target);
#endif
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
#if CONFIG_LVGL_USE_IMG
    nordic_semi_logo = lv_img_create(scr_welcome, NULL);
    lv_img_set_src(nordic_semi_logo, &nordic_semi_w150px);
    lv_obj_align(nordic_semi_logo, scr_welcome, LV_ALIGN_CENTER, 0, -20);
    nordic_semi_text = lv_img_create(scr_welcome, NULL);
    lv_img_set_src(nordic_semi_text, &nordic_semi_text_w150px);
    lv_obj_align(nordic_semi_text, nordic_semi_logo, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
#else
    label_welcome = lv_label_create(scr_welcome, NULL);
    lv_label_set_long_mode(label_welcome, LV_LABEL_LONG_BREAK);
    lv_label_set_align(label_welcome, LV_LABEL_ALIGN_CENTER);
    lv_label_set_text(label_welcome, "Welcome to Password Manager!");
    lv_obj_set_width(label_welcome, DISP_WIDTH-20);
    lv_obj_align(label_welcome, NULL, LV_ALIGN_CENTER, 0, 0);
#endif

    /* Init platform select screen */
    scr_select_platform = lv_obj_create(NULL, NULL);
    lv_obj_add_style(scr_select_platform, LV_OBJ_PART_MAIN, &style_screen);

    label_select_platform = lv_label_create(scr_select_platform, NULL);
    lv_label_set_long_mode(label_select_platform, LV_LABEL_LONG_EXPAND);
    lv_label_set_align(label_select_platform, LV_LABEL_ALIGN_LEFT);
    lv_label_set_text(label_select_platform, "No platforms found.");
    lv_obj_align(label_select_platform, scr_select_platform, LV_ALIGN_CENTER,0,0);
    list_select_platform = lv_list_create(scr_select_platform, NULL);
    list_group_select_platform = lv_group_create();
    lv_group_add_obj(list_group_select_platform, list_select_platform);

    /* Init fingerprint register screen */
    scr_fingerprint = lv_obj_create(NULL, NULL);
    // TODO: arc for fingerprint progress?

    /* Init transmission screen */
    scr_transmit = lv_obj_create(NULL, NULL);
    lv_obj_add_style(scr_transmit, LV_OBJ_PART_MAIN, &style_screen);
    // TODO: Add bluetooth signal?
    label_transmit_info = lv_label_create(scr_transmit, NULL);
    lv_label_set_align(label_transmit_info, LV_LABEL_ALIGN_CENTER);
    lv_label_set_text(label_transmit_info, "Inputting password...");
    lv_obj_align(label_transmit_info, scr_transmit, LV_ALIGN_CENTER, 0, 0);
}

//========================================================================================
/*                                                                                      *
 *                                 Execution of commands                                *
 *                              called from display module                              *
 *                                                                                      */
//========================================================================================
void lvgl_widgets_init(void)
{
    build_pages();
    change_screen(scr_welcome);
}

struct display_data get_highlighted_option(void) 
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

void set_platform_list_contents(const char *platform_names)
{
    lv_label_set_text(label_select_platform, "Platform select");
    lv_obj_align(label_select_platform, scr_select_platform, LV_ALIGN_IN_TOP_LEFT, 10, 10);

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

void set_screen(enum scr_state_type to_screen_state)
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
void scroll_platform_list(bool up)
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