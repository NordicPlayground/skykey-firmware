#include "display/display_ui.h"

///////////////////// VARIABLES ////////////////////
lv_obj_t * Welcome;
lv_obj_t * nordic_semi_logo;
lv_obj_t * nordic_semi_text;
lv_obj_t * Main;
lv_obj_t * password_manager_label;
lv_obj_t * list1;
lv_group_t * list_group;
lv_obj_t * btn;

lv_indev_t *indev;
lv_indev_drv_t indev_drv;

lv_indev_data_t button_press_data;


///////////////////// IMAGES ////////////////////
LV_IMG_DECLARE(nordic_semi_w150px);   // assets\nordic_semi_w150px.png
LV_IMG_DECLARE(nordic_semi_text_w150px);   // assets\nordic_semi_text_w150px.png

///////////////////// FUNCTIONS ////////////////////
void ChangeScreen(lv_obj_t * target, int fademode, int spd, int delay)
{
    lv_scr_load_anim(target, fademode, spd, delay, false);
}
///////////////////// HW BUTTONS /////////////////////
// bool read_latest_button_data(lv_indev_data_t *data) {
//     data->state = button_press_data.state;
//     data->btn_id = button_press_data.btn_id;
//     return false;
// }

// void hw_button_init(void)
// {
//   lv_indev_drv_init(&indev_drv);

//   indev_drv.type = LV_INDEV_TYPE_BUTTON;
//   indev_drv.read_cb = read_latest_button_data;
//   indev = lv_indev_drv_register(&indev_drv);


//   const lv_point_t btn_map[] = { 
//       { 10, 10 },
//       { 30, 10 },
//       { 50, 10 } 
//     };
//   lv_indev_set_button_points(indev, btn_map);
// }

void hw_button_pressed(uint32_t key_id) {
    if (key_id == 0) {
        lv_group_send_data(list_group, LV_KEY_DOWN);
        // button_press_data.state= LV_INDEV_STATE_PR;
        // button_press_data.btn_id = 0;
    } else if (key_id == 1) {
        lv_group_send_data(list_group, LV_KEY_UP);
        // button_press_data.state= LV_INDEV_STATE_PR;
        // button_press_data.btn_id = 1;
    }

}

void hw_button_released() {
    // button_press_data.state = LV_INDEV_STATE_REL;
}

void lv_folder_list(void) {
    /*Create a list*/
    list1 = lv_list_create(Main, NULL);
    lv_obj_set_size(list1, 220, 180);
    lv_obj_align(list1, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, -10);

    /*Add buttons to the list*/
    lv_obj_t * list_btn;


    list_btn = lv_list_add_btn(list1, LV_SYMBOL_FILE, "Newsssdsadsdsadsadsadwg");
    // lv_obj_set_event_cb(list_btn, event_handler);

    list_btn = lv_list_add_btn(list1, LV_SYMBOL_DIRECTORY, "Open");
    // lv_obj_set_event_cb(list_btn, event_handler);

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

    lv_list_set_edge_flash(list1, true);

    list_group = lv_group_create();
    lv_group_add_obj(list_group, list1);
}

// static void btn_click_action(lv_obj_t *btn, lv_event_t event) {
//     if (event == LV_EVENT_CLICKED) {
//         lv_group_send_data(list_group, LV_KEY_UP);
//     }
// }

//////////////////// INPUT DEVICES ////////////////////
// void init_input_device_driver(void) {
//     lv_indev_drv_init(&indev_drv);
//     indev_drv.type


// }



///////////////////// SCREENS ////////////////////

void BuildPages(void)
{
    Welcome = lv_obj_create(NULL, NULL);

    nordic_semi_logo = lv_img_create(Welcome, NULL);
    lv_img_set_src(nordic_semi_logo, &nordic_semi_w150px);
    lv_obj_align(nordic_semi_logo, Welcome, LV_ALIGN_CENTER, 0, -20);

    nordic_semi_text = lv_img_create(Welcome, NULL);
    lv_img_set_src(nordic_semi_text, &nordic_semi_text_w150px);

    Main = lv_obj_create(NULL, NULL);

    password_manager_label = lv_label_create(Main, NULL);
    lv_label_set_long_mode(password_manager_label, LV_LABEL_LONG_EXPAND);
    lv_label_set_align(password_manager_label, LV_LABEL_ALIGN_LEFT);
    lv_label_set_text(password_manager_label, "Password Manager");
    lv_obj_align(password_manager_label, Main, LV_ALIGN_IN_TOP_LEFT, 10, 10); // force: 137
    lv_folder_list();
    
    // /* Input buttons */
    // btn = lv_btn_create(Main, NULL);
    // lv_obj_set_size(btn, 20, 20);
    // lv_obj_set_pos(btn, 0, 0);
    // lv_obj_set_event_cb(btn, btn_click_action);

    /* Set background color to white */
	static lv_style_t style_screen;
	lv_style_init(&style_screen);
	lv_style_set_bg_color(&style_screen, LV_STATE_DEFAULT, LV_COLOR_WHITE);
	lv_obj_add_style(Welcome, LV_OBJ_PART_MAIN, &style_screen);
    lv_obj_add_style(Main, LV_OBJ_PART_MAIN, &style_screen);
}



// void BuildPages(void)
// {
//     Welcome = lv_obj_create(NULL, NULL);

//     nordic_semi_logo = lv_img_create(Welcome, NULL);
//     lv_img_set_src(nordic_semi_logo, &nordic_semi_w150px);
//     lv_obj_set_click(nordic_semi_logo, false);
//     lv_obj_set_hidden(nordic_semi_logo, false);
//     lv_obj_set_size(nordic_semi_logo, 150, 131);
//     lv_obj_align(nordic_semi_logo, Welcome, LV_ALIGN_CENTER, 0, -20);
//     lv_obj_set_drag(nordic_semi_logo, false);

//     lv_obj_clear_state(nordic_semi_logo, LV_STATE_DISABLED);

//     nordic_semi_text = lv_img_create(Welcome, NULL);
//     lv_img_set_src(nordic_semi_text, &nordic_semi_text_w150px);
//     lv_obj_set_click(nordic_semi_text, false);
//     lv_obj_set_hidden(nordic_semi_text, false);
//     lv_obj_set_size(nordic_semi_text, 150, 45);
//     lv_obj_align(nordic_semi_text, Welcome, LV_ALIGN_CENTER, 0, 75);
//     lv_obj_set_drag(nordic_semi_text, false);

//     lv_obj_clear_state(nordic_semi_text, LV_STATE_DISABLED);

//     Main = lv_obj_create(NULL, NULL);

//     password_manager_label = lv_label_create(Main, NULL);
//     lv_label_set_long_mode(password_manager_label, LV_LABEL_LONG_EXPAND);
//     lv_label_set_align(password_manager_label, LV_LABEL_ALIGN_CENTER);
//     lv_label_set_text(password_manager_label, "Password Manager");
//     lv_obj_set_size(password_manager_label, 137, 16);  // force: -42
//     lv_obj_set_click(password_manager_label, false);
//     lv_obj_set_hidden(password_manager_label, false);
//     lv_obj_clear_state(password_manager_label, LV_STATE_DISABLED);
//     lv_obj_set_drag(password_manager_label, false);

//     lv_obj_align(password_manager_label, Main, LV_ALIGN_CENTER, -42, -105); // force: 137

//     /* Set background color to white */
// 	static lv_style_t style_screen;
// 	lv_style_init(&style_screen);
// 	lv_style_set_bg_color(&style_screen, LV_STATE_DEFAULT, LV_COLOR_WHITE);
// 	lv_obj_add_style(Welcome, LV_OBJ_PART_MAIN, &style_screen);
//     lv_obj_add_style(Main, LV_OBJ_PART_MAIN, &style_screen);
// }

