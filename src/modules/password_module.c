/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr.h>
#include <event_manager.h>

#include <device.h>
#include <fs/fs.h>
#include <fs/littlefs.h>
#include <storage/flash_map.h>
#include <stdio.h>
#include <string.h>

#include "events/display_module_event.h"
#include "events/download_module_event.h"
#include "events/password_module_event.h"

#define MODULE password_module
#include <caf/events/power_event.h>
#include <caf/events/module_state_event.h>
#include <caf/events/power_manager_event.h>

#include "modules_common.h"
#include "util/file_util.h"
#include "util/parse_util.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_PASSWORD_MODULE_LOG_LEVEL);

#if IS_ENABLED(CONFIG_DOWNLOAD_MODULE)
BUILD_ASSERT(CONFIG_DOWNLOAD_FILE_MAX_SIZE_BYTES == CONFIG_PASSWORD_FILE_MAX_SIZE_BYTES);
#endif

struct password_msg_data
{
	union
	{
        struct display_module_event display;
		struct download_module_event download;
	} module;
};

/* Password module message queue. */
#define PASSWORD_QUEUE_ENTRY_COUNT 10
#define PASSWORD_QUEUE_BYTE_ALIGNMENT 4

K_MSGQ_DEFINE(msgq_password, sizeof(struct password_msg_data),
			  PASSWORD_QUEUE_ENTRY_COUNT, PASSWORD_QUEUE_BYTE_ALIGNMENT);


static struct module_data self = {
	.name = "password",
	.msg_q = &msgq_password,
	.supports_shutdown = true,
};

static enum state_type { STATE_READING,
						 STATE_SHUTDOWN,
						 STATE_IDLE,
} module_state = STATE_IDLE;

static enum sub_state_type { SUB_STATE_SHUTDOWN_PENDING,
							 SUB_STATE_DEFAULT,
} module_sub_state = SUB_STATE_DEFAULT;

static char *state2str(enum state_type new_state)
{
	switch (new_state)
	{
	case STATE_READING:
		return "STATE_READING";
	case STATE_IDLE:
		return "STATE_IDLE";
	case STATE_SHUTDOWN:
		return "STATE_SHUTDOWN";
	default:
		return "Unknown";
	}
}
static char *sub_state2str(enum sub_state_type new_sub_state)
{
	switch (new_sub_state)
	{
	case SUB_STATE_DEFAULT:
		return "SUB_STATE_DEFAULT";
	case SUB_STATE_SHUTDOWN_PENDING:
		return "SUB_STATE_SHUTDOWN_PENDING";
	default:
		return "Unknown";
	}
}

static void sub_state_set(enum sub_state_type new_sub_state)
{
	if (new_sub_state == module_sub_state)
	{
		LOG_DBG("State: %s", sub_state2str(module_sub_state));
		return;
	}

	LOG_DBG("State transition: %s --> %s",
			sub_state2str(module_sub_state),
			sub_state2str(new_sub_state));
	module_sub_state = new_sub_state;
}

static void state_set(enum state_type new_state)
{
	if (new_state == module_state)
	{
		LOG_DBG("State: %s", state2str(module_state));
		return;
	}

	if (new_state == STATE_IDLE && module_state == STATE_SHUTDOWN)
	{
		module_set_state(MODULE_STATE_READY);
	}

	if (module_sub_state == SUB_STATE_SHUTDOWN_PENDING)
	{
		sub_state_set(SUB_STATE_DEFAULT);
		state_set(STATE_SHUTDOWN);
		module_set_state(MODULE_STATE_OFF);
	}

	LOG_DBG("State transition: %s --> %s",
			state2str(module_state),
			state2str(new_state));
	module_state = new_state;
}

//========================================================================================
/*                                                                                      *
 *                                    Read file                                         *
 *                                                                                      */
//========================================================================================

#define READ_BUF_LEN CONFIG_PASSWORD_FILE_MAX_SIZE_BYTES /* Maximum amount of characters read from password file */
#define ENTRIES_BUF_MAX_LEN (CONFIG_PASSWORD_ENTRY_MAX_NUM) * (CONFIG_PASSWORD_ENTRY_NAME_MAX_LEN + 1)

static uint8_t encrypted_buf[READ_BUF_LEN];
static uint8_t plaintext_buf[READ_BUF_LEN];
uint8_t entries_buf[ENTRIES_BUF_MAX_LEN];


int decrypt_file(void) {
	//TODO: actually decrypt an encrypted file
	memcpy(plaintext_buf, encrypted_buf, READ_BUF_LEN);
	return 0;
}

/**
 * Scans plaintext buffer for platforms. Right now, it assumes that the 
 * platform entry is the first word on each new line.
 * @return Negative ERRNO on failure. 0 on success.
*/
int get_available_accounts(void)
{
	int err;
	err = decrypt_file();
	if (err)
	{
		LOG_WRN("Could not decrypt file.");
		return err;
	}
	memset(entries_buf, '\0', ENTRIES_BUF_MAX_LEN * sizeof(uint8_t));
	err = parse_entries(plaintext_buf, entries_buf, ENTRIES_BUF_MAX_LEN, ENTRY_ACCOUNT);
	return err;
}

/**
 * Scans plaintext buffer for platforms. Right now, it assumes that the 
 * platform entry is the first word on each new line.
 * @return Negative ERRNO on failure. 0 on success.
*/
int get_available_passwords(void)
{
	int err;
	err = decrypt_file();
	if (err)
	{
		LOG_WRN("Could not decrypt file.");
		return err;
	}
	memset(entries_buf, '\0', ENTRIES_BUF_MAX_LEN * sizeof(uint8_t));
	err = parse_entries(plaintext_buf, entries_buf, ENTRIES_BUF_MAX_LEN, ENTRY_PASSWORD);
	return err;
}

//========================================================================================
/*                                                                                      *
 *                                    State handlers/                                   *
 *                                 transition functions                                 *
 *                                                                                      */
//========================================================================================
// static void on_state_reading(struct password_msg_data *msg) {
// 	return;
// }

// static void on_state_idle(struct password_msg_data *msg)
// {

// 	if (IS_EVENT(msg, display, DISPLAY_EVT_REQUEST_PLATFORMS))
// 	{
// 		file_extract_content(encrypted_buf, READ_BUF_LEN);
// 		get_available_accounts();
// 		struct password_module_event *event = new_password_module_event();
// 		event->type = PASSWORD_EVT_READ_PLATFORMS;
// 		memcpy(event->data.entries, entries_buf, ENTRIES_BUF_MAX_LEN * sizeof(uint8_t));
// 		EVENT_SUBMIT(event);
// 	}
// 	if (IS_EVENT(msg, display, DISPLAY_EVT_PLATFORM_CHOSEN))
// 	{
// 		decrypt_file();
// 		char choice[CONFIG_PASSWORD_ENTRY_NAME_MAX_LEN];
// 		strncpy(choice, msg->module.display.data.choice, CONFIG_PASSWORD_ENTRY_NAME_MAX_LEN);
// 		char password[100]; //TODO: Make configurable
// 		get_password(plaintext_buf, choice, password, 100);
// 		LOG_DBG("Password: %s", log_strdup(password));
// 	}
// 	if (IS_EVENT(msg, download, DOWNLOAD_EVT_DOWNLOAD_FINISHED))
// 	{
// 		/* Temporarily react to this event. Should react to DISPLAY_EVT_REQUEST_PLATFORMS 
// 			   when we start supporting folder structure.*/
// 		file_extract_content(encrypted_buf, READ_BUF_LEN);
// 		get_available_accounts();
// 		struct password_module_event *event = new_password_module_event();
// 		event->type = PASSWORD_EVT_READ_PLATFORMS;
// 		memcpy(event->data.entries, entries_buf, ENTRIES_BUF_MAX_LEN * sizeof(uint8_t));
// 		EVENT_SUBMIT(event);
// 	}
// }
// 	return;
// }

// static void on_state_shutdown(struct password_msg_data *msg)
// {
// 	return;
// }

	//========================================================================================
	/*                                                                                      *
 *                                    Event handlers                                    *
 *                                                                                      */
	//========================================================================================

	/**
 * @brief Event manager event handler
 * 
 * @param eh Event header
 */
	static bool event_handler(const struct event_header *eh)
{
	struct password_msg_data msg = {0};
	bool enqueue_msg = false;

	if (is_download_module_event(eh))
	{
		struct download_module_event *evt = cast_download_module_event(eh);
		msg.module.download = *evt;
		enqueue_msg = true;
		state_set(STATE_READING);
	}

	if (is_display_module_event(eh))
	{
		struct display_module_event *evt = cast_display_module_event(eh);
		msg.module.display = *evt;
		enqueue_msg = true;
		state_set(STATE_READING);
	}

	if (IS_ENABLED(CONFIG_CAF_POWER_MANAGER) && is_power_down_event(eh))
	{
		if (module_state == STATE_READING)
		{
			// Consume event if we are reading file. Must unmount file system first!
			LOG_DBG("Currently reading file. Consumed power down event");
			sub_state_set(SUB_STATE_SHUTDOWN_PENDING);
			return true;
		}
		else
		{
			state_set(STATE_SHUTDOWN);
		}
	}

	if (IS_ENABLED(CONFIG_CAF_POWER_MANAGER) && is_wake_up_event(eh))
	{
		sub_state_set(SUB_STATE_DEFAULT);
		state_set(STATE_IDLE);
	}

	if (enqueue_msg)
	{
		int err = module_enqueue_msg(&self, &msg);

		if (err)
		{
			LOG_ERR("Message could not be enqueued");
			SEND_ERROR(password, PASSWORD_EVT_ERROR, err);
		}
	}

	return false;
}

/**
 * @brief Setup function for the module. Initializes modem and AWS connection.
 * 
 * @return int 0 on success, negative errno code on failure.
 */
static int setup(void)
{
	return 0;
}

//========================================================================================
/*                                                                                      *
 *                                     Module thread                                    *
 *                                                                                      */
//========================================================================================

static void module_thread_fn(void)
{
	LOG_DBG("Password module thread started");
	int err;
	struct password_msg_data msg;
	self.thread_id = k_current_get();

	err = module_start(&self);
	if (err)
	{
		LOG_ERR("Failed starting module, error: %d", err);
		SEND_ERROR(password, PASSWORD_EVT_ERROR, err);
	}

	err = setup();
	if (err)
	{
		LOG_ERR("setup, error %d", err);
		SEND_ERROR(password, PASSWORD_EVT_ERROR, err);
	}
	while (true)
	{
		if (module_state != STATE_SHUTDOWN) {
			module_get_next_msg(&self, &msg, K_FOREVER);
			if (IS_EVENT((&msg), display, DISPLAY_EVT_REQUEST_PLATFORMS))
			{
				file_extract_content(encrypted_buf, READ_BUF_LEN);
				get_available_accounts();
				struct password_module_event *event = new_password_module_event();
				event->type = PASSWORD_EVT_READ_PLATFORMS;
				memcpy(event->data.entries, entries_buf, ENTRIES_BUF_MAX_LEN * sizeof(uint8_t));
				EVENT_SUBMIT(event);
			}
			if (IS_EVENT((&msg), display, DISPLAY_EVT_PLATFORM_CHOSEN))
			{
				decrypt_file();
				char choice[CONFIG_PASSWORD_ENTRY_NAME_MAX_LEN];
				strncpy(choice, msg.module.display.data.choice, CONFIG_PASSWORD_ENTRY_NAME_MAX_LEN);
				char password[100]; //TODO: Make configurable
				get_password(plaintext_buf, choice, password, 100);
				LOG_DBG("Password: %s", log_strdup(password));
			}
			if (IS_EVENT((&msg), download, DOWNLOAD_EVT_DOWNLOAD_FINISHED))
			{
				/* Temporarily react to this event. Should react to DISPLAY_EVT_REQUEST_PLATFORMS 
			   when we start supporting folder structure.*/
				file_extract_content(encrypted_buf, READ_BUF_LEN);
				get_available_accounts();
				struct password_module_event *event = new_password_module_event();
				event->type = PASSWORD_EVT_READ_PLATFORMS;
				memcpy(event->data.entries, entries_buf, ENTRIES_BUF_MAX_LEN * sizeof(uint8_t));
				EVENT_SUBMIT(event);
			}
			state_set(STATE_IDLE);
		}
	}
}

K_THREAD_DEFINE(password_module_thread, CONFIG_PASSWORD_THREAD_STACK_SIZE,
				module_thread_fn, NULL, NULL, NULL,
				K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);

EVENT_LISTENER(MODULE, event_handler);
EVENT_SUBSCRIBE(MODULE, display_module_event);
EVENT_SUBSCRIBE(MODULE, download_module_event);
#if IS_ENABLED(CONFIG_CAF_POWER_MANAGER)
EVENT_SUBSCRIBE_EARLY(MODULE, power_down_event);
EVENT_SUBSCRIBE(MODULE, wake_up_event);
#endif