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
#include <settings/settings.h>

#include <caf/events/click_event.h>

#define MODULE display_module

#include "modules_common.h"
#include "display/display_ui.h"
#include "events/display_module_event.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_DISPLAY_MODULE_LOG_LEVEL);

struct display_msg_data {
    union {
        struct click_event btn;
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

	if (enqueue_msg) {
		int err = module_enqueue_msg(&self, &msg);
		if (err) {
			LOG_ERR("Message could not be queued");
			SEND_ERROR(display, DISPLAY_EVT_ERROR, err);
		}
	}
    return false;
}

static void module_thread_fn(void) 
{
	int err;
	struct display_msg_data msg;
	const struct device *display_dev;
	display_dev = device_get_binding(CONFIG_LVGL_DISPLAY_DEV_NAME);
	self.thread_id = k_current_get();

	err = module_start(&self);
	if (err) {
		LOG_ERR("Failed starting module, error: %d", err);
		SEND_ERROR(display, DISPLAY_EVT_ERROR, err); 
	}

	// err = setup() {
	// 	LOG_ERR("setup, error: %d", err);
	// }

	if (display_dev == NULL) {
		LOG_ERR("device not found.  Aborting test.");
		return;
	}


	lvgl_widgets_init();

	lv_task_handler();
	display_blanking_off(display_dev);
	while (1) {
		int err = module_get_next_msg(&self, &msg, K_MSEC(5));
		if (err == 0) {
			if (msg.module.btn.click == CLICK_LONG) {
				const struct display_data feedback = 
					hw_button_long_pressed(msg.module.btn.key_id);
				if (feedback.id == DISPLAY_PLATFORM_CHOSEN) {
					struct display_module_event *display_module_event = 
						new_display_module_event();
					
					display_module_event->data.choice.platform = feedback.data;
					display_module_event->type = DISPLAY_PLATFORM_CHOSEN;
					EVENT_SUBMIT(display_module_event);
				}
			} else {
				hw_button_pressed(msg.module.btn.key_id);
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