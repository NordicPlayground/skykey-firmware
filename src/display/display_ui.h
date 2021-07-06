#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <lvgl.h>

struct display_data {
	uint8_t id;
	char* data;
};

enum display_data_type {
    DISPLAY_NO_DATA,
    DISPLAY_FOLDER_CHOSEN,
    DISPLAY_PLATFORM_CHOSEN
};

static enum btn_id_type {
	BTN_DOWN,
	BTN_UP
} btn_id;

void lvgl_widgets_init(void);
void hw_button_pressed(uint32_t key_id);
struct display_data hw_button_long_pressed(uint32_t key_id);

LV_IMG_DECLARE(nordic_semi_w150px);   // assets\nordic_semi_w150px.c
LV_IMG_DECLARE(nordic_semi_text_w150px);   // assets\nordic_semi_text_w150px.c

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
