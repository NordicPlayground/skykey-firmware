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
#include <caf/events/module_state_event.h>

#define MODULE display_module

#include "modules_common.h"
#include "display/lvgl_interface.h"
#include "events/display_module_event.h"
#include "events/password_module_event.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_DISPLAY_MODULE_LOG_LEVEL);

#if IS_ENABLED(CONFIG_PASSWORD_MODULE)
BUILD_ASSERT(CONFIG_DISPLAY_LIST_ENTRY_MAX_NUM == CONFIG_PASSWORD_ENTRY_MAX_NUM);
BUILD_ASSERT(CONFIG_DISPLAY_LIST_ENTRY_MAX_LEN == CONFIG_PASSWORD_ENTRY_NAME_MAX_LEN);
#endif

struct display_msg_data {

	/* module_id needed to detect click event (cannot use IS_EVENT macro) */
	int module_id;
    union {
        struct click_event btn;
		struct password_module_event password;
    } module;
};

enum module_id {
	CLICK_EVENT,
	PASSWORD_MODULE_EVENT
};

/* Display module states. */
static enum state_type { STATE_ACTIVE,
						 STATE_SHUTDOWN
} state;

/**
 * Numbers describe the index of the buttons defined in buttons_def.h
*/
enum btn_id_type
{
	BTN_DOWN,
	BTN_UP
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

static enum scr_state_type scr_state = SCR_STATE_WELCOME;

static char *state2str(enum scr_state_type new_state)
{
	switch (new_state)
	{
	case SCR_STATE_WELCOME:
		return "SCR_STATE_WELCOME";
	case SCR_STATE_FINGERPRINT:
		return "SCR_STATE_FINGERPRINT";
	case SCR_STATE_SELECT_PLATFORM:
		return "SCR_STATE_SELECT_PLATFORM";
	case SCR_STATE_TRANSMIT:
		return "SCR_STATE_TRANSMIT";
	default:
		return "SCR_STATE_NONE";
	}
}

static void scr_state_set(enum scr_state_type new_state)
{
	if (new_state == scr_state)
	{
		LOG_DBG("State: %s", state2str(scr_state));
		return;
	}

	set_screen(new_state);

	LOG_DBG("State transition: %s --> %s",
			state2str(scr_state),
			state2str(new_state));
	scr_state = new_state;
}

//========================================================================================
/*                                                                                      *
 *                                    State handlers/                                   *
 *                                 transition functions                                 *
 *                                                                                      */
//========================================================================================

static void on_state_scr_welcome(struct display_msg_data *msg) {
	if (msg->module_id == CLICK_EVENT) {
		scr_state_set(SCR_STATE_SELECT_PLATFORM);
	}
}

static void on_state_scr_select_platform(struct display_msg_data *msg)
{
	if (msg->module_id == CLICK_EVENT) 
	{
		int button_pressed = msg->module.btn.key_id;
		int click = msg->module.btn.click;
		if (button_pressed == BTN_DOWN) {
			if (click == CLICK_LONG) {
				scr_state_set(SCR_STATE_WELCOME);
			} else {
				scroll_platform_list(false);
			}
		} else if (button_pressed == BTN_UP) {
			if (click == CLICK_LONG) {
				const struct display_data feedback =
					get_highlighted_option();
				if (feedback.id == DISPLAY_PLATFORM_CHOSEN){
					struct display_module_event *display_module_event =
						new_display_module_event();

					strncpy(display_module_event->data.choice, feedback.data, CONFIG_DISPLAY_LIST_ENTRY_MAX_LEN);
					display_module_event->type = DISPLAY_EVT_PLATFORM_CHOSEN;
					EVENT_SUBMIT(display_module_event);
					scr_state_set(SCR_STATE_TRANSMIT);
				}
			} else {
				scroll_platform_list(true);
			}
		}
	}
}

static void on_state_scr_fingerprint(struct display_msg_data *msg) 
{
	return;
}

static void on_state_scr_transmit(struct display_msg_data *msg) 
{
	if (msg->module_id == CLICK_EVENT) {
		//TODO: remove this. For testing purposes only.
		scr_state_set(SCR_STATE_SELECT_PLATFORM);
	}
	
	return;
}

static void on_all_states(struct display_msg_data *msg) 
{
	if (IS_EVENT(msg, password, PASSWORD_EVT_READ_PLATFORMS))
	{
		set_platform_list_contents((const char *)msg->module.password.data.entries);
	}
	return;
}

//========================================================================================
/*                                                                                      *
 *                                    Event handlers                                    *
 *                                                                                      */
//========================================================================================
static bool handle_power_down_event(const struct power_down_event *event)
{
	switch (state)
	{
		case STATE_ACTIVE:
			state = STATE_SHUTDOWN;
			module_set_state(MODULE_STATE_OFF);
		break;
		case STATE_SHUTDOWN:
		break;
	// case STATE_DISABLED:
	// 	state = STATE_DISABLED_STANDBY;
	// 	break;

	// case STATE_ERASE_PEER:
	// case STATE_ERASE_ADV:
	// case STATE_SELECT_PEER:
	// 	cancel_operation();
	// 	/* Fall-through */

	// case STATE_IDLE:
	// 	state = STATE_STANDBY;
	// 	module_set_state(MODULE_STATE_OFF);
	// 	break;

	// case STATE_DONGLE_CONN:
	// 	state = STATE_DONGLE_CONN_STANDBY;
	// 	module_set_state(MODULE_STATE_OFF);
	// 	break;

	// case STATE_STANDBY:
	// 	/* Fall-through */
	// case STATE_DISABLED_STANDBY:
	// 	/* Fall-through */
	// case STATE_DONGLE_CONN_STANDBY:
	// 	/* No action. */
	// 	break;

	default:
		__ASSERT_NO_MSG(false);
		break;
	}

	return false;
}

static bool event_handler(const struct event_header *eh)
{
	struct display_msg_data msg = {0};
	bool enqueue_msg = false;
    if (is_click_event(eh)) {
        struct click_event *event = cast_click_event(eh);
		msg.module_id = CLICK_EVENT;
        msg.module.btn = *event;
		enqueue_msg = true;
    };

	if (is_password_module_event(eh)) {
		struct password_module_event *event = cast_password_module_event(eh);
		msg.module_id = PASSWORD_MODULE_EVENT;
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

//========================================================================================
/*                                                                                      *
 *                                    Threads                                           *
 *                                                                                      */
//========================================================================================

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

	while (true) {
		int err = module_get_next_msg(&self, &msg, K_MSEC(5));
		if (!err) {
			// if (IS_ENABLED(CONFIG_CAF_POWER_MANAGER) &&
			// 	is_power_down_event(eh))
			// {
			// 	return handle_power_down_event(cast_power_down_event(eh));
			// }

			// if (IS_ENABLED(CONFIG_CAF_POWER_MANAGER) &&
			// 	is_wake_up_event(eh))
			// {
			// 	return handle_wake_up_event(cast_wake_up_event(eh));
			// }
			switch (scr_state) {
				case SCR_STATE_WELCOME:
				on_state_scr_welcome(&msg);
				break;
				case SCR_STATE_SELECT_PLATFORM:
				on_state_scr_select_platform(&msg);
				break;
				case SCR_STATE_TRANSMIT:
				on_state_scr_transmit(&msg);
				break;
				case SCR_STATE_FINGERPRINT:
				on_state_scr_fingerprint(&msg);
				break;
				default:
				break;
			}
			on_all_states(&msg);
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
#if IS_ENABLED(CONFIG_CAF_POWER_MANAGER)
EVENT_SUBSCRIBE(MODULE, power_down_event);
EVENT_SUBSCRIBE(MODULE, wake_up_event);
#endif