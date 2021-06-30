#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <lvgl.h>

extern lv_obj_t * Welcome;
extern lv_obj_t * nordic_semi_logo;
extern lv_obj_t * nordic_semi_text;
extern lv_obj_t * Main;
extern lv_obj_t * password_manager_label;
extern lv_obj_t * list1;
extern lv_group_t * list_group;

void BuildPages(void);
void ChangeScreen(lv_obj_t *, int, int, int);
void hw_button_pressed(uint32_t);
void hw_button_released();

LV_IMG_DECLARE(nordic_semi_w150px);   // assets\nordic_semi_w150px.c
LV_IMG_DECLARE(nordic_semi_text_w150px);   // assets\nordic_semi_text_w150px.c

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
