#include "display/display_ui.h"
#include <zephyr.h>

///////////////////// VARIABLES ////////////////////
lv_obj_t * scr_welcome;
lv_obj_t * nordic_semi_logo;
lv_obj_t * nordic_semi_text;
lv_obj_t * label_welcome;

lv_obj_t * scr_select_folder;
lv_obj_t * select_folder_label;
lv_obj_t * select_folder_list;
lv_group_t * select_folder_list_group;

lv_obj_t * scr_select_platform;
lv_obj_t * select_platform_label;
lv_obj_t * select_platform_list;
lv_group_t * select_platform_list_group;

lv_style_t * style_scr;
lv_style_t * style_list;

lv_color_t nordic_blue = LV_COLOR_MAKE(0x7f,0xd4,0xe6);

enum scr_index {
    SCR_NONE,
    SCR_WELCOME,
    SCR_SELECT_FOLDER,
    SCR_SELECT_PLATFORM,
};

///////////////////// SIZE ////////////////////
#define DISP_WIDTH CONFIG_LVGL_HOR_RES_MAX
#define DISP_HEIGHT CONFIG_LVGL_VER_RES_MAX

///////////////////// IMAGES ////////////////////
#if CONFIG_LVGL_USE_IMG
LV_IMG_DECLARE(nordic_semi_w150px);   // assets\nordic_semi_w150px.png
LV_IMG_DECLARE(nordic_semi_text_w150px);   // assets\nordic_semi_text_w150px.png
#endif

///////////////////// FUNCTIONS ////////////////////
void change_screen(lv_obj_t * target, int fademode, int spd, int delay)
{
#if CONFIG_LVGL_USE_ANIMATION
    lv_scr_load_anim(target, fademode, spd, delay, false);
#else
    lv_scr_load(target);
#endif
}

int get_scr_index(lv_obj_t* scr) {
    if (scr == scr_welcome) {
        return SCR_WELCOME;
    } else if (scr == scr_select_folder) {
        return SCR_SELECT_FOLDER;
    } else if (scr == scr_select_platform) {
        return SCR_SELECT_PLATFORM;
    } else {
        return SCR_NONE;
    }
}

///////////////////// COMPONENT BUILDING ////////////////////

void generate_list(lv_obj_t *list, const char opts[CONFIG_DISPLAY_LIST_ENTRY_MAX_NUM][CONFIG_DISPLAY_LIST_ENTRY_MAX_LEN], const int num_opts)
{
    /*Create a list*/
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

//Not in use right now. Should be used from folder screen eventually
void set_scr_platform_select(const char *folder_name)
{
    lv_label_set_text(select_platform_label, "Select platform");
    select_platform_list = lv_list_create(scr_select_platform, NULL);
    select_platform_list_group = lv_group_create();
    lv_group_add_obj(select_platform_list_group, select_platform_list);
}

void set_platform_list_contents(const char *platform_names)
{
    lv_list_clean(select_platform_list);
    /*Create a list*/
    lv_obj_set_size(select_platform_list, DISP_WIDTH - 20, DISP_HEIGHT - 50);
    lv_obj_align(select_platform_list, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, -10);

    static lv_style_t style_btn;
    lv_style_init(&style_btn);
    lv_style_set_bg_color(&style_btn, LV_BTN_STATE_DISABLED, nordic_blue);
    lv_style_set_value_color(&style_btn, LV_BTN_STATE_DISABLED, LV_COLOR_WHITE);
    /*Add buttons to the list*/
    lv_obj_t *list_btn;

    char opts[CONFIG_DISPLAY_LIST_ENTRY_MAX_NUM][CONFIG_DISPLAY_LIST_ENTRY_MAX_LEN];
    int num_opts = 0;
    char* token = strtok((char*)platform_names, " ");
    while (token != NULL)
    {
        printk("%s\n",token);
        strncpy(opts[num_opts], token, CONFIG_DISPLAY_LIST_ENTRY_MAX_LEN);
        num_opts++;
        if (num_opts > CONFIG_DISPLAY_LIST_ENTRY_MAX_NUM) {
            // LOG_ERR("Number of platform options exceed the maximum configurated value. Consider increasing CONFIG_DISPLAY_LIST_ENTRY_MAX_NUM.");
            break;
        }
        token = strtok(NULL, " ");
    }
    generate_list(select_platform_list, opts, num_opts);
    return;
}

    void set_scr_folder_select(const char *folder_opts[], const int num_opts)
{
    select_folder_list = lv_list_create(scr_select_folder, NULL);
    // generate_list(select_folder_list, folder_opts, num_opts);
    select_folder_list_group = lv_group_create();
    lv_group_add_obj(select_folder_list_group, select_folder_list);
}

///////////////////// SCREENS ////////////////////
void build_pages(void)
{
    /* Set background color to white */
	static lv_style_t style_screen;
	lv_style_init(&style_screen);
	lv_style_set_bg_color(&style_screen, LV_STATE_DEFAULT, LV_COLOR_WHITE);

    /* Welcome screen */
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
    /* Folder select screen */
    scr_select_folder = lv_obj_create(NULL, NULL);
    lv_obj_add_style(scr_select_folder, LV_OBJ_PART_MAIN, &style_screen);
    select_folder_label = lv_label_create(scr_select_folder, NULL);
    lv_label_set_long_mode(select_folder_label, LV_LABEL_LONG_EXPAND);
    lv_label_set_align(select_folder_label, LV_LABEL_ALIGN_LEFT);
    lv_label_set_text(select_folder_label, "Folder select");
    lv_obj_align(select_folder_label, scr_select_folder, LV_ALIGN_IN_TOP_LEFT, 10, 10);

    /* Platform select screen */
    scr_select_platform = lv_obj_create(NULL, NULL);
    lv_obj_add_style(scr_select_platform, LV_OBJ_PART_MAIN, &style_screen);
    select_platform_label = lv_label_create(scr_select_platform, NULL);
    lv_label_set_long_mode(select_platform_label, LV_LABEL_LONG_EXPAND);
    lv_label_set_align(select_platform_label, LV_LABEL_ALIGN_LEFT);
    lv_label_set_text(select_platform_label, "Platform select");
    lv_obj_align(select_platform_label, scr_select_platform, LV_ALIGN_IN_TOP_LEFT, 10, 10);
    select_platform_list = lv_list_create(scr_select_platform, NULL);
    select_platform_list_group = lv_group_create();
    lv_group_add_obj(select_platform_list_group, select_platform_list);
}

void lvgl_widgets_init(void) {
    build_pages();
	change_screen(scr_welcome, LV_SCR_LOAD_ANIM_FADE_ON, 2000, 0);
}


///////////////////// HW BUTTONS /////////////////////
void hw_button_pressed(uint32_t btn_id) {
    int scr_index = get_scr_index(lv_scr_act());
    switch (scr_index) {
        case SCR_WELCOME:
            // change_screen(scr_select_folder, LV_SCR_LOAD_ANIM_MOVE_LEFT, 1000, 0);
            change_screen(scr_select_platform, LV_SCR_LOAD_ANIM_MOVE_LEFT, 1000, 0);
        break;
        case SCR_SELECT_FOLDER:
        if (btn_id == BTN_DOWN) {
            lv_group_send_data(select_folder_list_group, LV_KEY_DOWN);
        } else if (btn_id == BTN_UP) {
            lv_group_send_data(select_folder_list_group, LV_KEY_UP);
        }
        break;
        case SCR_SELECT_PLATFORM:
        if (btn_id == BTN_DOWN) {
            lv_group_send_data(select_platform_list_group, LV_KEY_DOWN);
        } else if (btn_id == BTN_UP) {
            lv_group_send_data(select_platform_list_group, LV_KEY_UP);
        }
        break;
    }
}

struct display_data hw_button_long_pressed(uint32_t btn_id) {
    struct display_data info = {
        .id = DISPLAY_NO_DATA,
        .data = ""
    };
    int scr_index = get_scr_index(lv_scr_act());
    switch (scr_index) {
        case SCR_WELCOME:
            // change_screen(scr_select_folder, LV_SCR_LOAD_ANIM_MOVE_LEFT, 1000, 0);
            change_screen(scr_select_platform, LV_SCR_LOAD_ANIM_MOVE_LEFT, 1000, 0);
        break;
        case SCR_SELECT_FOLDER:
        if (btn_id == BTN_UP) {
            char* selected_folder = lv_label_get_text(lv_list_get_btn_label(lv_list_get_btn_selected(select_folder_list)));
            if (!strcmp(selected_folder, "Welcome!")) {
                change_screen(scr_welcome, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 1000, 0);
            } else {
                // set_scr_platform_select(selected_folder);
                change_screen(scr_select_platform, LV_SCR_LOAD_ANIM_MOVE_LEFT, 1000, 0);
            }
        } else if (btn_id == BTN_DOWN) {
            change_screen(scr_welcome, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 1000, 0);
        }
        break;
        case SCR_SELECT_PLATFORM:
        if (btn_id == BTN_UP) {
            info.id = DISPLAY_PLATFORM_CHOSEN;
            char *chosen_label = lv_label_get_text(lv_list_get_btn_label(lv_list_get_btn_selected(select_platform_list)));

            strncpy(info.data, chosen_label, CONFIG_DISPLAY_LIST_ENTRY_MAX_LEN);
            // info.data = lv_label_get_text(lv_list_get_btn_label(lv_list_get_btn_selected(select_platform_list)));
        } else if (btn_id == BTN_DOWN) {
            // /* Go to the top of the list, change the screen */
            // lv_list_focus_btn(select_folder_list, lv_list_get_next_btn(select_folder_list, NULL));
            // change_screen(scr_select_folder, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 1000, 0);

            /*For now, support only single level structure*/
            lv_list_focus_btn(select_platform_list, lv_list_get_next_btn(select_platform_list, NULL));
            change_screen(scr_welcome, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 1000, 0);
        }
        break;
    }
    return info;
}