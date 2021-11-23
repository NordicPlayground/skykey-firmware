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
#include <sys/reboot.h>

#include <caf/events/click_event.h>
#include <caf/events/module_state_event.h>

#define MODULE display_module

#include "modules_common.h"
#include "display/lvgl_interface.h"
#include "events/display_module_event.h"
#include "events/password_module_event.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_DISPLAY_MODULE_LOG_LEVEL);

#if IS_ENABLED(CONFIG_CAF_POWER_MANAGER)
#include <caf/events/power_event.h>
#include <sys_clock.h>	
#endif

#if IS_ENABLED(CONFIG_PASSWORD_MODULE)
BUILD_ASSERT(CONFIG_DISPLAY_LIST_ENTRY_MAX_NUM == CONFIG_PASSWORD_ENTRY_MAX_NUM);
BUILD_ASSERT(CONFIG_DISPLAY_LIST_ENTRY_MAX_LEN == CONFIG_PASSWORD_ENTRY_NAME_MAX_LEN);
#endif

// Forward declarations
#if IS_ENABLED(CONFIG_CAF_POWER_MANAGER)
static bool handle_power_down_event(struct power_down_event *event, const struct device * display_dev);
static bool handle_wake_up_event(struct wake_up_event* event, const struct device* display_dev);
#endif

K_TIMER_DEFINE(elapsed_time, NULL, NULL);
int enroll_number = 0; // For testing purposes only. This will come from the fingerprint module
struct display_msg_data {

	/* module_id needed to detect click, wakeup 
	and powerdown events (cannot use IS_EVENT macro) */
	int module_id;
    union {
        struct click_event btn;
		struct password_module_event password;
		#if IS_ENABLED(CONFIG_CAF_POWER_MANAGER)
		struct power_down_event power_down;
		struct wake_up_event wake_up;
		#endif
    } module;
};

enum module_id
{
	CLICK_EVENT,
	PASSWORD_MODULE_EVENT,
#if IS_ENABLED(CONFIG_CAF_POWER_MANAGER)
	POWER_DOWN_EVENT,
	WAKE_UP_EVENT
#endif
};

/* Display module states. */
static enum state_type { STATE_ACTIVE,
						 STATE_SHUTDOWN
} state = STATE_ACTIVE;

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

static char *scr_state2str(enum scr_state_type new_state)
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
		LOG_DBG("Screen state: %s", scr_state2str(scr_state));
		return;
	}

	disp_set_screen(new_state);

	LOG_DBG("Screen state transition: %s --> %s",
			scr_state2str(scr_state),
			scr_state2str(new_state));
	scr_state = new_state;
}

static char *state2str(enum state_type new_state)
{
	switch (new_state)
	{
	case STATE_ACTIVE:
		return "STATE_ACTIVE";
	case STATE_SHUTDOWN:
		return "STATE_SHUTDOWN";
	default:
		return "Unknown";
	}
}

static void state_set(enum state_type new_state)
{
	if (new_state == state)
	{
		LOG_DBG("State: %s", state2str(state));
		return;
	}

	if (new_state == STATE_SHUTDOWN) {
		scr_state_set(SCR_STATE_WELCOME);
	}
	// if (new_state == STATE_ACTIVE) {
	// 	scr_state_set(SCR_STATE_WELCOME);
	// }

	LOG_DBG("State transition: %s --> %s",
			state2str(state),
			state2str(new_state));
	state = new_state;
}

int boot_display(const struct device *display_dev)
{
	int err = 0;
	err = device_is_ready(display_dev);
	if (!err)
	{
		LOG_ERR("Device not ready: %d", err);
	}

	err = display_blanking_off(display_dev);
	if (err)
	{
		LOG_ERR("Display blanking error: %d", err);
	}

	display_set_brightness(display_dev, 255);
	struct display_module_event *display_module_event =
		new_display_module_event();
	LOG_DBG("Device usable: %d", device_usable_check(display_dev));
	display_module_event->type = DISPLAY_EVT_REQUEST_PLATFORMS;
	EVENT_SUBMIT(display_module_event);
	return 0;
}

int power_down_display(const struct device *display_dev)
{
	LOG_DBG("Powering down display");
	display_set_brightness(display_dev, 0);
	display_blanking_on(display_dev);
	return 0;
}

//========================================================================================
/*                                                                                      *
 *                                    State handlers/                                   *
 *                                 transition functions                                 *
 *                                                                                      */
//========================================================================================

static void on_state_scr_welcome(struct display_msg_data *msg) {
	if (msg->module_id == CLICK_EVENT) {
		scr_state_set(SCR_STATE_FINGERPRINT);
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
				disp_scroll_platform_list(false);
			}
		} else if (button_pressed == BTN_UP) {
			if (click == CLICK_LONG) {
				const struct display_data feedback =
					disp_get_highlighted_option();
				if (feedback.id == DISPLAY_PLATFORM_CHOSEN){
					struct display_module_event *display_module_event =
						new_display_module_event();

					strncpy(display_module_event->data.choice, feedback.data, CONFIG_DISPLAY_LIST_ENTRY_MAX_LEN);
					display_module_event->type = DISPLAY_EVT_PLATFORM_CHOSEN;
					EVENT_SUBMIT(display_module_event);
					scr_state_set(SCR_STATE_TRANSMIT);
				}
			} else {
				disp_scroll_platform_list(true);
			}
		}
	}
}

static void on_state_scr_fingerprint(struct display_msg_data *msg) 
{
	if (msg->module_id == CLICK_EVENT)
	{
		if (enroll_number > 4) {
			scr_state_set(SCR_STATE_SELECT_PLATFORM);
			enroll_number = 0;
		} else {
			disp_set_fingerprint_progress(enroll_number);
			enroll_number++;
		}

	}
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

static void on_all_scr_states(struct display_msg_data *msg, const struct device *display_dev)
{
	if (msg->module_id == WAKE_UP_EVENT) {
		handle_wake_up_event(&(msg->module.wake_up), display_dev);
	}
	if (msg->module_id == POWER_DOWN_EVENT) {
		handle_power_down_event(&(msg->module.power_down), display_dev);
	}
	if (IS_EVENT(msg, password, PASSWORD_EVT_READ_PLATFORMS))
	{
		disp_set_platform_list_contents((const char *)msg->module.password.data.entries);
	}

	return;
}

//========================================================================================
/*                                                                                      *
 *                                    Event handlers                                    *
 *                                                                                      */
//========================================================================================
static bool handle_power_down_event(struct power_down_event *event, const struct device *display_dev)
{
	switch (state)
	{
		case STATE_ACTIVE:
			power_down_display(display_dev);
			k_timer_stop(&elapsed_time);
			state_set(STATE_SHUTDOWN);
			lv_task_handler();
		break;
		case STATE_SHUTDOWN:
		break;
	default:
		__ASSERT_NO_MSG(false);
		break;
	}

	return false;
}

static bool handle_wake_up_event(struct wake_up_event *event, const struct device *display_dev)
{
	switch (state)
	{
		case STATE_ACTIVE:
			break;
		case STATE_SHUTDOWN:
			state_set(STATE_ACTIVE);
			enroll_number = 0;
			disp_set_fingerprint_progress(enroll_number);
			k_timer_start(&elapsed_time, K_SECONDS(CONFIG_CAF_POWER_MANAGER_TIMEOUT), K_NO_WAIT);
			boot_display(display_dev);

			break;
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

	if (is_wake_up_event(eh))
	{
		struct wake_up_event *event = cast_wake_up_event(eh);
		msg.module_id = WAKE_UP_EVENT;
		msg.module.wake_up = *event;
		enqueue_msg = true;
	}

	if ((state == STATE_SHUTDOWN) && !enqueue_msg) {
		// Ignore messages if we are shut down and there has not been a wake up event
		return false;
	}

	if (is_power_down_event(eh))
	{
		struct power_down_event *event = cast_power_down_event(eh);
		msg.module_id = POWER_DOWN_EVENT;
		msg.module.power_down = *event;
		enqueue_msg = true;
	}

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

	const struct device *display_dev;
	display_dev = device_get_binding(CONFIG_LVGL_DISPLAY_DEV_NAME);
	disp_widgets_init();
	err = boot_display(display_dev);

	if (err) {
		SEND_ERROR(display, DISPLAY_EVT_ERROR, err);
	}

	err = module_start(&self);
	if (err) {
		LOG_ERR("Failed starting module, error: %d", err);
		SEND_ERROR(display, DISPLAY_EVT_ERROR, err); 
	}
	k_timer_start(&elapsed_time, K_SECONDS(CONFIG_CAF_POWER_MANAGER_TIMEOUT), K_NO_WAIT);
	disp_set_timer(k_timer_remaining_get(&elapsed_time) / 1000);
	lv_task_handler();


	while (true) {
		if (state == STATE_SHUTDOWN) {
			err = module_get_next_msg(&self, &msg, K_FOREVER);
		} else {
			err = module_get_next_msg(&self, &msg, K_MSEC(5));
		}
		if (!err) {
			switch (scr_state) {
				case SCR_STATE_WELCOME:
					on_state_scr_welcome(&msg);
					break;
				case SCR_STATE_FINGERPRINT:
					on_state_scr_fingerprint(&msg);
					break;
				case SCR_STATE_SELECT_PLATFORM:
					on_state_scr_select_platform(&msg);
					break;
				case SCR_STATE_TRANSMIT:
					on_state_scr_transmit(&msg);
					break;
				default:
				break;
			}
			on_all_scr_states(&msg, display_dev);
		}
		disp_set_timer(k_timer_remaining_get(&elapsed_time) / 1000);
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
EVENT_SUBSCRIBE_EARLY(MODULE, power_down_event);
EVENT_SUBSCRIBE(MODULE, wake_up_event);
#endif