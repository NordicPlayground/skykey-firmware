/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr.h>
#include <event_manager.h>
#include <zephyr/types.h>
#include <string.h>
#include <stdlib.h>
#include <device.h>

#include <net/download_client.h>

#include <drivers/flash.h>
#include <settings/settings.h>
#include <storage/stream_flash.h>


#include "events/password_module_event.h"
#include "events/cloud_module_event.h"

#define MODULE password_module

#include "modules_common.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_PASSWORD_MODULE_LOG_LEVEL);

struct password_msg_data
{
    union
    {
        struct password_module_event password;
        struct cloud_module_event cloud;
    } module;
	void *data;
};

static enum state_type { STATE_DOWNLOADING,
              STATE_FREE,
} module_state = STATE_FREE;

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

static struct download_client dl_client;
static struct download_client_cfg dl_client_cfg = {
	.sec_tag = -1,
	.apn = NULL,
	.pdn_id = 0,
	.frag_size_override = 0,
	.set_tls_hostname = false,
};

static bool first_fragment;
static int socket_retries_left;

static char *state2str(enum state_type new_state)
{
	switch (new_state)
	{
	case STATE_DOWNLOADING:
		return "STATE_DOWNLOADING";
	case STATE_FREE:
		return "STATE_FREE";
	default:
		return "Unknown";
	}
}

static void state_set(enum state_type new_state)
{
	if (new_state == module_state)
	{
		LOG_DBG("State: %s", state2str(module_state));
		return;
	}

	LOG_DBG("State transition: %s --> %s",
			state2str(module_state),
			state2str(new_state));
	module_state = new_state;
}

static int download_connect_and_start(const char* url) 
{
    int err = download_client_connect(&dl_client, url, &dl_client_cfg);
	if (err != 0) {
		return err;
	}

	err = download_client_start(&dl_client, url, 0);
	if (err != 0) {
		return err;
	}

	socket_retries_left = CONFIG_PASSWORD_DOWNLOAD_SOCKET_RETRIES;
    state_set(STATE_DOWNLOADING);
	return err;
}

//========================================================================================
/*                                                                                      *
 *                                    Stream flash                                      *
 *                                                                                      */
//========================================================================================

#define STORAGE_NODE DT_NODE_BY_FIXED_PARTITION_LABEL(storage)
#define FLASH_NAME DT_CHOSEN_ZEPHYR_FLASH_CONTROLLER_LABEL


#define BUF_LEN CONFIG_DOWNLOAD_CLIENT_HTTP_FRAG_SIZE
#define MAX_PAGE_SIZE BUF_LEN /* Buf len cannot be larger than the page size*/


/* so that we don't overwrite the application when running on hw */
#define FLASH_BASE DT_REG_ADDR(STORAGE_NODE)
#define FLASH_AVAILABLE DT_REG_SIZE(STORAGE_NODE) /* Assume only pw module writes to storage node */

static const struct device *fdev;
static struct stream_flash_ctx ctx;

static uint8_t buf[BUF_LEN];



static int init_stream_flash(void) 
{
	int err = 0;

	/* Ensure that target is clean */
	memset(&ctx, 0, sizeof(ctx));
	memset(buf, 0, BUF_LEN);

	err = stream_flash_init(&ctx, fdev, buf, BUF_LEN, FLASH_BASE, 0, NULL);
	if (err < 0) {
		return err;
	}
	return err;
}


static int handle_file_fragment(const void * const fragment, size_t frag_size, size_t file_size)
{
    int err; 
	
	memcpy(buf, fragment, frag_size);

    static int frag_count = 0;
    static int data_received = 0;
    data_received += frag_size;
	bool should_flush = false;
	if (data_received == file_size) {
		should_flush = true;
	}
	err = stream_flash_buffered_write(&ctx, buf, frag_size, should_flush);
	if (err != 0) {
		return err;
	}

    int percentage = (data_received * 100) / file_size;
    LOG_DBG("Received fragment %d.\n Received: %d B/%d B\n  (%d%%)", frag_count++, data_received, file_size, percentage);
    return err;
}

//========================================================================================
/*                                                                                      *
 *                                    State handlers/                                   *
 *                                 transition functions                                 *
 *                                                                                      */
//========================================================================================

static void on_state_free(struct password_msg_data *msg)
{
	int err;
    if (IS_EVENT(msg, cloud, CLOUD_EVT_DATABASE_UPDATE_AVAILABLE)) {
		err = download_connect_and_start(msg->data);
		if (err != 0) {
			SEND_DYN_ERROR(password, PASSWORD_EVT_DOWNLOAD_ERROR, err);
			return;
		}
		SEND_DYN_EVENT(password, PASSWORD_EVT_DOWNLOAD_STARTED);


	}
}

static void on_state_downloading(struct password_msg_data *msg)
{
}

/* Message handler for all states. */
static void on_all_states(struct password_msg_data *msg)
{
    //TODO: Implement or remove
    return;
}

//========================================================================================
/*                                                                                      *
 *                                    Event handlers                                    *
 *                                                                                      */
//========================================================================================

static int download_client_callback(const struct download_client_evt *event)
{
	static size_t file_size;
	int err;
	switch (event->id) {
	case DOWNLOAD_CLIENT_EVT_FRAGMENT: {
		if (first_fragment) {
			LOG_DBG("Fragment received. Size: %d", event->fragment.len);
			err = download_client_file_size_get(&dl_client, &file_size);
			if (err != 0) {
				LOG_DBG("download_client_file_size_get err: %d", err);
				return err;
			}
			first_fragment = false;
		}
		handle_file_fragment(event->fragment.buf, event->fragment.len, file_size);
		break;
	}
	case DOWNLOAD_CLIENT_EVT_DONE: {
		SEND_DYN_EVENT(password, PASSWORD_EVT_DOWNLOAD_FINISHED);
        state_set(STATE_FREE);
		download_client_disconnect(&dl_client);
		LOG_DBG("Stream flash bytes written: %d", stream_flash_bytes_written(&ctx));
		LOG_DBG("Download complete");
		first_fragment = true;
		break;
	}
	case DOWNLOAD_CLIENT_EVT_ERROR: {
		/* In case of socket errors we can return 0 to retry/continue,
		 * or non-zero to stop
		 */
		if ((socket_retries_left) && ((event->error == -ENOTCONN) ||
					      (event->error == -ECONNRESET))) {
			LOG_WRN("Download socket error. %d retries left...", socket_retries_left);
			socket_retries_left--;
			/* Fall through and return 0 below to tell
			 * download_client to retry
			 */
		} else {
			download_client_disconnect(&dl_client);
			first_fragment = true;
			int err = event->error; /* Glue for logging */
			SEND_DYN_ERROR(password, PASSWORD_EVT_DOWNLOAD_ERROR, err);
			LOG_ERR("An error occured while downloading: %d", err);
			return err;
		}
		break;
	}
	}
	return 0;
}

/**
 * @brief Event manager event handler
 * 
 * @param eh Event header
 * @return true 
 * @return false 
 */
static bool event_handler(const struct event_header *eh)
{
    struct password_msg_data msg = {0};
    bool enqueue_msg = false;

    if (is_cloud_module_event(eh)) {
        struct cloud_module_event *evt = cast_cloud_module_event(eh);

        msg.module.cloud = *evt;
		if (evt->dyndata.size > 0) {
			msg.data = k_malloc(evt->dyndata.size);
			memcpy(msg.data, evt->dyndata.data, evt->dyndata.size);
		}
        enqueue_msg = true;
    }

    if (enqueue_msg) {
        int err = module_enqueue_msg(&self, &msg);

        if (err) {
            LOG_ERR("Message could not be enqueued");
            SEND_DYN_ERROR(password, PASSWORD_EVT_ERROR, err);
        }
    }

    return false;
}



//========================================================================================
/*                                                                                      *
 *                                  Setup/configuration                                 *
 *                                                                                      */
//========================================================================================

/**
 * @brief Setup function for the module. 
 * 
 * @return int 0 on success, negative errno code on failure.
 */
static int setup(void)
{
    int err = 0;
	fdev = device_get_binding(FLASH_NAME);
	if (fdev == NULL) {
		return -ENODEV;
	}

	err = download_client_init(&dl_client, download_client_callback);
	if (err != 0) {
		return err;
	}
	err = init_stream_flash();
	if (err != 0) {
		return err;
	}

	state_set(STATE_FREE);
	first_fragment = true;
	socket_retries_left = CONFIG_PASSWORD_DOWNLOAD_SOCKET_RETRIES;
    return err;
}

//========================================================================================
/*                                                                                      *
 *                                     Module thread                                    *
 *                                                                                      */
//========================================================================================

static void module_thread_fn(void)
{
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
    while (true)
    {
        module_get_next_msg(&self, &msg, K_FOREVER);
        switch (module_state)
        {
        case STATE_FREE:
        {
            on_state_free(&msg);

            break;
        }
        case STATE_DOWNLOADING:
        {
            on_state_downloading(&msg);
            break;
        }
        default:
            LOG_ERR("Unknown state %d", module_state);
        }
        on_all_states(&msg);
    }
}

K_THREAD_DEFINE(password_module_thread, CONFIG_PASSWORD_THREAD_STACK_SIZE,
                module_thread_fn, NULL, NULL, NULL,
                K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);

EVENT_LISTENER(MODULE, event_handler);
EVENT_SUBSCRIBE(MODULE, cloud_module_event);