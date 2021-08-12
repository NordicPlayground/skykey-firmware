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

#include "events/display_module_event.h"
#include "events/download_module_event.h"
#include "events/password_module_event.h"

#define MODULE password_module

#include "modules_common.h"
#include "util/file_util.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_PASSWORD_MODULE_LOG_LEVEL);

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

#define READ_BUF_LEN 256 /* Maximum amount of characters read from password file */
#define MAX_NUM_PLATFORMS 10
#define MAX_STRING_SIZE 10
#define PLATFORMS_BUF_MAX_LEN MAX_NUM_PLATFORMS*MAX_STRING_SIZE+MAX_NUM_PLATFORMS

static uint8_t read_buf[READ_BUF_LEN];
static uint8_t plaintext_buf[READ_BUF_LEN];
struct platforms {
	uint8_t platforms_buf[PLATFORMS_BUF_MAX_LEN];
	size_t size;
} platforms;


// static uint8_t platforms[PLATFORMS_BUF_MAX_LEN];

int decrypt_file(void) {
	//TODO: actually decrypt an encrypted file
	memcpy(plaintext_buf, read_buf, READ_BUF_LEN);
	return 0;
}

/* Very basic scanning of the plaintext buf */
void get_available_platforms(void) {
	decrypt_file();
	memset(platforms.platforms_buf, '0', PLATFORMS_BUF_MAX_LEN);
	platforms.size = 0;
	int read_i = 0;
	int platforms_i = 0;

	//TODO: Find a smarter way to do this scanning
	while (read_i < READ_BUF_LEN) {
		if (plaintext_buf[read_i] == ' ')
		{ //First entry of line is fully read
			platforms.platforms_buf[platforms_i] = ' ';
			platforms_i++;
			while (plaintext_buf[read_i] != '\n') {
				if (plaintext_buf[read_i] == '0' || read_i == READ_BUF_LEN-1) {
					platforms.size = platforms_i;
					return;
				}
				read_i++;
			}
		} else {
			if (plaintext_buf[read_i] == '0')
			{ //End of contents
				platforms.platforms_buf[platforms_i-1] ='0'; //remove trailing space
				platforms.size = platforms_i;
				return;
			}
			platforms.platforms_buf[platforms_i] = plaintext_buf[read_i];
			read_i++;
			platforms_i++;
		}

	}
	memset(plaintext_buf, '0', READ_BUF_LEN); // Don't keep plaintext buffer for too long
	return;
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
			SEND_DYN_ERROR(password, PASSWORD_EVT_ERROR, err);
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
		SEND_DYN_ERROR(password, PASSWORD_EVT_ERROR, err);
	}

	err = setup();
	if (err)
	{
		LOG_ERR("setup, error %d", err);
		SEND_DYN_ERROR(password, PASSWORD_EVT_ERROR, err);
	}
	/*Here temporarily for testing purposes*/
	// file_extract_content(read_buf, READ_BUF_LEN);
	// get_available_platforms();
	// for (int i = 0; i < PLATFORMS_BUF_MAX_LEN; i++)
	// {
	// 	printk("%c", platforms.platforms_buf[i]);
	// }
	// printk("platforms.size: %d\n", platforms.size);
	// struct password_module_event *event = new_password_module_event(platforms.size); // +1 for null terminator.
	// event->type = PASSWORD_EVT_READ_PLATFORMS;
	// memcpy(event->dyndata.data, &(platforms.platforms_buf), platforms.size);
	// EVENT_SUBMIT(event);
	while (true)
	{

		module_get_next_msg(&self, &msg, K_FOREVER);
		if (IS_EVENT((&msg), display, DISPLAY_EVT_REQUEST_PLATFORMS)) {

		}
		if (IS_EVENT((&msg), download, DOWNLOAD_EVT_DOWNLOAD_FINISHED)) {
			//Temporarily react to this event. Should react to DISPLAY_EVT_REQUEST_PLATFORMS
			file_extract_content(read_buf, READ_BUF_LEN);
			get_available_platforms();
			// printk("platforms.size: %d\n", platforms.size);
			struct password_module_event *event = new_password_module_event(platforms.size);
			event->type = PASSWORD_EVT_READ_PLATFORMS;
			memcpy(event->dyndata.data, &(platforms.platforms_buf), platforms.size);
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