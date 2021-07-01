#include "display/display_ui.h"

///////////////////// VARIABLES ////////////////////
lv_obj_t * welcome_scrn;
lv_obj_t * nordic_semi_logo;
lv_obj_t * nordic_semi_text;
lv_obj_t * select_scrn;
lv_obj_t * password_manager_label;
lv_obj_t * list1;
lv_group_t * list_group;
lv_obj_t * btn;
lv_theme_t * theme;

lv_indev_t *indev;
lv_indev_drv_t indev_drv;

lv_indev_data_t button_press_data;

///////////////////// IMAGES ////////////////////
LV_IMG_DECLARE(nordic_semi_w150px);   // assets\nordic_semi_w150px.png
LV_IMG_DECLARE(nordic_semi_text_w150px);   // assets\nordic_semi_text_w150px.png

///////////////////// FUNCTIONS ////////////////////
void change_screen(lv_obj_t * target, int fademode, int spd, int delay)
{
    lv_scr_load_anim(target, fademode, spd, delay, false);
}

static void event_handler(lv_obj_t * obj, lv_event_t event) {
    change_screen(welcome_scrn, LV_SCR_LOAD_ANIM_FADE_ON, 2000, 0);
    // if (event == LV_EVENT_CLICKED) {
    //     change_screen(welcome_scrn, LV_SCR_LOAD_ANIM_FADE_ON, 2000, 0);
    // }
}

///////////////////// HW BUTTONS /////////////////////
void hw_button_pressed(uint32_t key_id) {
    lv_obj_t * current_scrn = lv_scr_act();
    if (key_id == 0) {
        lv_group_send_data(list_group, LV_KEY_DOWN);
    } else if (key_id == 1) {
        lv_group_send_data(list_group, LV_KEY_UP);
    }
}
void hw_button_long_pressed(uint32_t key_id) {
    lv_obj_t * current_scrn = lv_scr_act();
    if (key_id == 0) {
        //lv_group_send_data(list_group, LV_KEY_DOWN);
        lv_group_send_data(list_group, LV_KEY_ENTER);
    } else if (key_id == 1) {
        // lv_obj_t* btn = lv_list_get_btn_selected(list1);
        // lv_list_set_btn_selected(btn);
        lv_group_send_data(list_group, LV_KEY_ENTER);
    }
}


///////////////////// NORDIC STYLE /////////////////////
// static void init_nordic_theme() {
//     lv_theme_t * base_theme = lv_theme_material_init(LV_THEME_DEFAULT_COLOR_PRIMARY, LV_THEME_DEFAULT_COLOR_SECONDARY, LV_THEME_DEFAULT_FLAG, LV_THEME_DEFAULT_FONT_SMALL, LV_THEME_DEFAULT_FONT_NORMAL, LV_THEME_DEFAULT_FONT_SUBTITLE, LV_THEME_DEFAULT_FONT_TITLE);
//     static lv_theme_t nordic_theme;
//     lv_theme_copy(&nordic_theme, base_theme);
//     static lv_style_t style_list;
//     lv_style_init(&style_list)
// }

// static void add_button_style(lv_obj_t *btn) 
// {
// 	static lv_style_t style_btn;
	
// 	//Style setting
// 	lv_style_init(&style_btn);
// 	//Default state
// 	lv_style_set_radius(&style_btn, LV_STATE_DEFAULT, 10); //Set rounded corners
// 	lv_style_set_shadow_opa(&style_btn, LV_STATE_DEFAULT, LV_OPA_30); //Set shadow transparency
// 	lv_style_set_bg_opa(&style_btn, LV_STATE_DEFAULT, LV_OPA_COVER); //Set background transparency
// 	lv_style_set_bg_color(&style_btn, LV_STATE_DEFAULT, LV_COLOR_YELLOW); //Set the background color
//     lv_style_set_bg_grad_color(&style_btn, LV_STATE_DEFAULT, LV_COLOR_GREEN); //Set the color of the bottom half of the background
//     lv_style_set_bg_grad_dir(&style_btn, LV_STATE_DEFAULT, LV_GRAD_DIR_VER); //Set the direction of the lower half
//     //pressed state
// 	lv_style_set_shadow_opa(&style_btn, LV_STATE_PRESSED, LV_OPA_30); //Set shadow transparency
// 	lv_style_set_bg_opa(&style_btn, LV_STATE_PRESSED, LV_OPA_COVER); //Set background transparency
// 	lv_style_set_bg_color(&style_btn, LV_STATE_PRESSED, LV_COLOR_GREEN); //Set the background color
//     lv_style_set_bg_grad_color(&style_btn, LV_STATE_PRESSED, LV_COLOR_YELLOW); //Set the color of the bottom half of the background
    
//     lv_style_set_bg_grad_dir(&style_btn, LV_STATE_PRESSED, LV_GRAD_DIR_VER); //Set the direction of the lower half
	
//     /*Add a border*/
//     lv_style_set_border_color(&style_btn, LV_STATE_DEFAULT, LV_COLOR_WHITE); //Add bounding box
//     lv_style_set_border_opa(&style_btn, LV_STATE_DEFAULT, LV_OPA_30); //Set the transparency of the defense box
//     lv_style_set_border_width(&style_btn, LV_STATE_DEFAULT, 2); //Set the width of the bounding box
	
// 	/*Different border color in focused state*/
//     lv_style_set_border_color(&style_btn, LV_STATE_FOCUSED, LV_COLOR_BLUE); //Set the color of the bounding box in the focused state
//     lv_style_set_border_color(&style_btn, LV_STATE_FOCUSED | LV_STATE_PRESSED, LV_COLOR_NAVY);//Set focus|Bounding box color when pressed
// }

void example_roller_1(void) {
    /*A style to make the selected option larger*/
    static lv_style_t style_sel;
    lv_style_init(&style_sel);
    lv_style_set_text_font(&style_sel, &lv_font_montserrat_22);


    const char * opts = "LOoooooooooooooong name\nGo to welcome screen\nFolder 1\nFolder 2\nFolder 3\nFolder 4"
    lv_obj_t *roller1 = lv_roller_create(select_scrn);
    lv_roller_set_options(roller1, opts, LV_ROLLER_MODE_INIFINITE);
    lv_roller_set_visible_row_count(roller1, 4);
    lv_roller_set_auto_fit(roller1, false);
    lv_
}

void lv_folder_list(void) {
    /*Create a list*/
    list1 = lv_list_create(select_scrn, NULL);
    lv_obj_set_size(list1, 220, 180);
    lv_obj_align(list1, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, -10);

    /*Add buttons to the list*/
    lv_obj_t * list_btn;
    list_btn = lv_list_add_btn(list1, LV_SYMBOL_FILE, "Go to the super duper mega cool welcome screen :D");
    lv_obj_set_event_cb(list_btn, event_handler);
    add_button_style(list_btn);

    list_btn = lv_list_add_btn(list1, LV_SYMBOL_DIRECTORY, "Open");
    add_button_style(list_btn);
    lv_obj_set_event_cb(list_btn, event_handler);
    lv_obj_add_event_cb()
    list_btn = lv_list_add_btn(list1, LV_SYMBOL_CLOSE, "Delete");
    // lv_obj_set_event_cb(list_btn, event_handler);

    list_btn = lv_list_add_btn(list1, LV_SYMBOL_EDIT, "Edit");
    // lv_obj_set_event_cb(list_btn, event_handler);

    list_btn = lv_list_add_btn(list1, LV_SYMBOL_SAVE, "Save");
    // lv_obj_set_event_cb(list_btn, event_handler);

    list_btn = lv_list_add_btn(list1, LV_SYMBOL_BELL, "Notify");
    // lv_obj_set_event_cb(list_btn, event_handler);

    list_btn = lv_list_add_btn(list1, LV_SYMBOL_BATTERY_FULL, "Battery");
    // lv_obj_set_event_cb(list_btn, event_handler);

    list_group = lv_group_create();
    lv_group_add_obj(list_group, list1);
}

///////////////////// SCREENS ////////////////////

void BuildPages(void)
{
    welcome_scrn = lv_obj_create(NULL, NULL);
    nordic_semi_logo = lv_img_create(welcome_scrn, NULL);
    lv_img_set_src(nordic_semi_logo, &nordic_semi_w150px);
    lv_obj_align(nordic_semi_logo, welcome_scrn, LV_ALIGN_CENTER, 0, -20);

    nordic_semi_text = lv_img_create(welcome_scrn, NULL);
    lv_img_set_src(nordic_semi_text, &nordic_semi_text_w150px);

    select_scrn = lv_obj_create(NULL, NULL);

    password_manager_label = lv_label_create(select_scrn, NULL);
    lv_label_set_long_mode(password_manager_label, LV_LABEL_LONG_EXPAND);
    lv_label_set_align(password_manager_label, LV_LABEL_ALIGN_LEFT);
    lv_label_set_text(password_manager_label, "Password Manager");
    lv_obj_align(password_manager_label, select_scrn, LV_ALIGN_IN_TOP_LEFT, 10, 10); // force: 137
    lv_folder_list();
    
    // /* Input buttons */
    // btn = lv_btn_create(select_scrn, NULL);
    // lv_obj_set_size(btn, 20, 20);
    // lv_obj_set_pos(btn, 0, 0);
    // lv_obj_set_event_cb(btn, btn_click_action);

    /* Set background color to white */
	// static lv_style_t style_screen;
	// lv_style_init(&style_screen);
	// lv_style_set_bg_color(&style_screen, LV_STATE_DEFAULT, LV_COLOR_WHITE);
	// lv_obj_add_style(welcome_scrn, LV_OBJ_PART_MAIN, &style_screen);
    // lv_obj_add_style(select_scrn, LV_OBJ_PART_MAIN, &style_screen);
}



// void BuildPages(void)
// {
//     welcome_scrn = lv_obj_create(NULL, NULL);

//     nordic_semi_logo = lv_img_create(welcome_scrn, NULL);
//     lv_img_set_src(nordic_semi_logo, &nordic_semi_w150px);
//     lv_obj_set_click(nordic_semi_logo, false);
//     lv_obj_set_hidden(nordic_semi_logo, false);
//     lv_obj_set_size(nordic_semi_logo, 150, 131);
//     lv_obj_align(nordic_semi_logo, welcome_scrn, LV_ALIGN_CENTER, 0, -20);
//     lv_obj_set_drag(nordic_semi_logo, false);

//     lv_obj_clear_state(nordic_semi_logo, LV_STATE_DISABLED);

//     nordic_semi_text = lv_img_create(welcome_scrn, NULL);
//     lv_img_set_src(nordic_semi_text, &nordic_semi_text_w150px);
//     lv_obj_set_click(nordic_semi_text, false);
//     lv_obj_set_hidden(nordic_semi_text, false);
//     lv_obj_set_size(nordic_semi_text, 150, 45);
//     lv_obj_align(nordic_semi_text, welcome_scrn, LV_ALIGN_CENTER, 0, 75);
//     lv_obj_set_drag(nordic_semi_text, false);

//     lv_obj_clear_state(nordic_semi_text, LV_STATE_DISABLED);

//     select_scrn = lv_obj_create(NULL, NULL);

//     password_manager_label = lv_label_create(select_scrn, NULL);
//     lv_label_set_long_mode(password_manager_label, LV_LABEL_LONG_EXPAND);
//     lv_label_set_align(password_manager_label, LV_LABEL_ALIGN_CENTER);
//     lv_label_set_text(password_manager_label, "Password Manager");
//     lv_obj_set_size(password_manager_label, 137, 16);  // force: -42
//     lv_obj_set_click(password_manager_label, false);
//     lv_obj_set_hidden(password_manager_label, false);
//     lv_obj_clear_state(password_manager_label, LV_STATE_DISABLED);
//     lv_obj_set_drag(password_manager_label, false);

//     lv_obj_align(password_manager_label, select_scrn, LV_ALIGN_CENTER, -42, -105); // force: 137

//     /* Set background color to white */
// 	static lv_style_t style_screen;
// 	lv_style_init(&style_screen);
// 	lv_style_set_bg_color(&style_screen, LV_STATE_DEFAULT, LV_COLOR_WHITE);
// 	lv_obj_add_style(welcome_scrn, LV_OBJ_PART_MAIN, &style_screen);
//     lv_obj_add_style(select_crn, LV_OBJ_PART_MAIN, &style_screen);
// }

