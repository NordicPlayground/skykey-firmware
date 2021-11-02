#ifndef LVGL_INTERFACE_H
#define LVGL_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <lvgl.h>
#include <sys/util.h>

struct display_data {
	uint8_t id;
	char data[CONFIG_DISPLAY_LIST_ENTRY_MAX_LEN];
};

enum display_data_type {
    DISPLAY_NO_DATA,
    DISPLAY_PLATFORM_CHOSEN
};

enum scr_state_type { SCR_STATE_WELCOME,
					 SCR_STATE_FINGERPRINT,
					 SCR_STATE_SELECT_PLATFORM,
					 SCR_STATE_TRANSMIT,
};

void lvgl_widgets_init(void);
void set_timer(int elapsed_time);
void set_platform_list_contents(const char *platform_names);
void set_screen(enum scr_state_type to_screen);
struct display_data get_highlighted_option(void);
void scroll_platform_list(bool up);
#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
