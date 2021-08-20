/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <device.h>
#include <drivers/display.h>
#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include <zephyr.h>
#include <kernel.h>
#include <event_manager.h>
#include <stdint.h>
#include <settings/settings.h>

#include <caf/events/click_event.h>

#define MODULE display_module

#include "modules_common.h"
#include "display/display_ui.h"
#include "events/display_module_event.h"
#include "events/password_module_event.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_DISPLAY_MODULE_LOG_LEVEL);

#if IS_ENABLED(CONFIG_PASSWORD_MODULE)
BUILD_ASSERT(CONFIG_DISPLAY_LIST_ENTRY_MAX_NUM == CONFIG_PASSWORD_ENTRY_MAX_NUM);
BUILD_ASSERT(CONFIG_DISPLAY_LIST_ENTRY_MAX_LEN == CONFIG_PASSWORD_ENTRY_NAME_MAX_LEN);
#endif

struct display_msg_data {
    union {
        struct click_event btn;
		struct password_module_event password;
    } module;
};



/* Display module message queue. */
#define DISPLAY_QUEUE_ENTRY_COUNT		10
#define DISPLAY_QUEUE_BYTE_ALIGNMENT	4

K_MSGQ_DEFINE(msgq_display, sizeof(struct display_msg_data),
	      DISPLAY_QUEUE_ENTRY_COUNT, DISPLAY_QUEUE_BYTE_ALIGNMENT);

static struct module_data self = {
	.name = "display",
	.msg_q = &msgq_display,
	.supports_shutdown = true,
};

static bool event_handler(const struct event_header *eh)
{
	struct display_msg_data msg = {0};
	bool enqueue_msg = false;
    if (is_click_event(eh)) {
        struct click_event *event = cast_click_event(eh);
        msg.module.btn = *event;
		enqueue_msg = true;
    };

	if (is_password_module_event(eh)) {
		struct password_module_event *event = cast_password_module_event(eh);

		msg.module.password = *event;
		enqueue_msg = true;
	}

	if (enqueue_msg) {
		int err = module_enqueue_msg(&self, &msg);
		if (err) {
			LOG_ERR("Message could not be queued");
			SEND_ERROR(display, DISPLAY_EVT_ERROR, err);
		}
	}
    return false;
}

int setup(void) {
	int err = 0;
	const struct device *display_dev;
	display_dev = device_get_binding(CONFIG_LVGL_DISPLAY_DEV_NAME);
	err = device_is_ready(display_dev);
	if (!err) {
		LOG_ERR("Device not ready: %d", err);
	}
	err = display_blanking_off(display_dev);
	if (err) {
		LOG_ERR("Display blanking error: %d", err);
	}
	lvgl_widgets_init();
	struct display_module_event *display_module_event =
		new_display_module_event();

	display_module_event->type = DISPLAY_EVT_REQUEST_PLATFORMS;
	EVENT_SUBMIT(display_module_event);
	return 0;
}

static void module_thread_fn(void) 
{
	LOG_DBG("Display module thread started");
	int err;
	struct display_msg_data msg;
	self.thread_id = k_current_get();
	
	err = setup();
	if (err) {
		SEND_ERROR(display, DISPLAY_EVT_ERROR, err);
	}

	err = module_start(&self);
	if (err) {
		LOG_ERR("Failed starting module, error: %d", err);
		SEND_ERROR(display, DISPLAY_EVT_ERROR, err); 
	}

	lv_task_handler();
	while (1) {
		int err = module_get_next_msg(&self, &msg, K_MSEC(5));
		if (!err) {
			if (IS_EVENT((&msg), password, PASSWORD_EVT_READ_PLATFORMS)) {
				LOG_WRN("PASSWORD_EVT_READ_PLATFORMS");
				set_platform_list_contents((const char*)msg.module.password.data.entries);
			} else { // TODO: find better way to check if it is click module
				if (msg.module.btn.click == CLICK_LONG)
				{
					const struct display_data feedback =
						hw_button_long_pressed(msg.module.btn.key_id);
					if (feedback.id == DISPLAY_PLATFORM_CHOSEN)
					{
						struct display_module_event *display_module_event =
							new_display_module_event();

						strncpy(display_module_event->data.choice, feedback.data, CONFIG_DISPLAY_LIST_ENTRY_MAX_LEN);
						display_module_event->type = DISPLAY_EVT_PLATFORM_CHOSEN;
						EVENT_SUBMIT(display_module_event);
					}
				}
				else
				{
					hw_button_pressed(msg.module.btn.key_id);
				}
			}
		}
		lv_task_handler();
	}


}

K_THREAD_DEFINE(display_module_thread, CONFIG_DISPLAY_THREAD_STACK_SIZE,
		module_thread_fn, NULL, NULL, NULL,
		K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);

EVENT_LISTENER(MODULE, event_handler);
EVENT_SUBSCRIBE(MODULE, click_event);
EVENT_SUBSCRIBE(MODULE, password_module_event);