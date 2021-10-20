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
	}

	if (is_display_module_event(eh))
	{
		struct display_module_event *evt = cast_display_module_event(eh);
		msg.module.display = *evt;
		enqueue_msg = true;
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
	/*Here temporarily for testing purposes*/
	while (true)
	{

		module_get_next_msg(&self, &msg, K_FOREVER);
		if (IS_EVENT((&msg), display, DISPLAY_EVT_REQUEST_PLATFORMS)) {
			file_extract_content(encrypted_buf, READ_BUF_LEN);
			get_available_accounts();
			struct password_module_event *event = new_password_module_event();
			event->type = PASSWORD_EVT_READ_PLATFORMS;
			memcpy(event->data.entries, entries_buf, ENTRIES_BUF_MAX_LEN * sizeof(uint8_t));
			EVENT_SUBMIT(event);
		}
		if (IS_EVENT((&msg), display, DISPLAY_EVT_PLATFORM_CHOSEN)) {
			decrypt_file();
			char choice[CONFIG_PASSWORD_ENTRY_NAME_MAX_LEN];
			strncpy(choice, msg.module.display.data.choice, CONFIG_PASSWORD_ENTRY_NAME_MAX_LEN);
			char password[100]; //TODO: Make configurable
			get_password(plaintext_buf, choice, password, 100);
			LOG_DBG("Password: %s", log_strdup(password));
		}
		if (IS_EVENT((&msg), download, DOWNLOAD_EVT_DOWNLOAD_FINISHED)) {
			/* Temporarily react to this event. Should react to DISPLAY_EVT_REQUEST_PLATFORMS 
			   when we start supporting folder structure.*/
			file_extract_content(encrypted_buf, READ_BUF_LEN);
			get_available_accounts();
			struct password_module_event *event = new_password_module_event();
			event->type = PASSWORD_EVT_READ_PLATFORMS;
			memcpy(event->data.entries, entries_buf, ENTRIES_BUF_MAX_LEN * sizeof(uint8_t));
			EVENT_SUBMIT(event);
		}
	}
}

K_THREAD_DEFINE(password_module_thread, CONFIG_PASSWORD_THREAD_STACK_SIZE,
				module_thread_fn, NULL, NULL, NULL,
				K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);

EVENT_LISTENER(MODULE, event_handler);
EVENT_SUBSCRIBE(MODULE, display_module_event);
EVENT_SUBSCRIBE(MODULE, download_module_event);