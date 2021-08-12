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

#include "util/file_util.h"

#include "events/download_module_event.h"
#include "events/cloud_module_event.h"

#define MODULE download_module

#include "modules_common.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_DOWNLOAD_MODULE_LOG_LEVEL);

struct download_msg_data
{
    union
    {
        struct download_module_event download;
        struct cloud_module_event cloud;
    } module;
	void *data;
};

static enum state_type { STATE_DOWNLOADING,
              STATE_FREE,
} module_state = STATE_FREE;

/* Download module message queue. */
#define DOWNLOAD_QUEUE_ENTRY_COUNT 10
#define DOWNLOAD_QUEUE_BYTE_ALIGNMENT 4

K_MSGQ_DEFINE(msgq_download, sizeof(struct download_msg_data),
              DOWNLOAD_QUEUE_ENTRY_COUNT, DOWNLOAD_QUEUE_BYTE_ALIGNMENT);

static struct module_data self = {
    .name = "download",
    .msg_q = &msgq_download,
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

	socket_retries_left = CONFIG_DOWNLOAD_DOWNLOAD_SOCKET_RETRIES;
    state_set(STATE_DOWNLOADING);
	return err;
}

static int handle_file_fragment(const void * const fragment, size_t frag_size, size_t file_size)
{
    int err; 

    static int frag_count = 0;
    static int data_received = 0;
    data_received += frag_size;

	err = file_write(fragment, frag_size);

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

static void on_state_free(struct download_msg_data *msg)
{
	int err;
    if (IS_EVENT(msg, cloud, CLOUD_EVT_DATABASE_UPDATE_AVAILABLE)) {


		err = download_connect_and_start(msg->data);
		if (err != 0) {
			SEND_ERROR(download, DOWNLOAD_EVT_ERROR, err);
			return;
		}
		SEND_EVENT(download, DOWNLOAD_EVT_DOWNLOAD_STARTED);


	}
}

static void on_state_downloading(struct download_msg_data *msg)
{
	return;
}

/* Message handler for all states. */
static void on_all_states(struct download_msg_data *msg)
{
    //TODO: Implement or remove
	if (msg->data != NULL) {
		k_free(msg->data); // Free dynamic data
	}
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
			err = file_write_start();
			if (err != 0)
			{
				SEND_ERROR(download, DOWNLOAD_EVT_FLASH_ERROR, err);
				return err;
			}
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
		download_client_disconnect(&dl_client);
		file_close_and_unmount();
		state_set(STATE_FREE);
		LOG_DBG("Download complete");
		first_fragment = true;
		SEND_EVENT(download, DOWNLOAD_EVT_DOWNLOAD_FINISHED);
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
			file_close_and_unmount();
			first_fragment = true;
			int err = event->error; /* Glue for logging */
			LOG_ERR("An error occured while downloading: %d", err);
			SEND_ERROR(download, DOWNLOAD_EVT_ERROR, err);
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
    struct download_msg_data msg = {0};
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
            SEND_ERROR(download, DOWNLOAD_EVT_ERROR, err);
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

	err = download_client_init(&dl_client, download_client_callback);
	if (err != 0) {
		return err;
	}

	state_set(STATE_FREE);
	first_fragment = true;
	socket_retries_left = CONFIG_DOWNLOAD_DOWNLOAD_SOCKET_RETRIES;
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
    struct download_msg_data msg;
    self.thread_id = k_current_get();

    err = module_start(&self);
    if (err)
    {
        LOG_ERR("Failed starting module, error: %d", err);
        SEND_ERROR(download, DOWNLOAD_EVT_ERROR, err);
    }

    err = setup();
    if (err)
    {
        LOG_ERR("setup, error %d", err);
        SEND_ERROR(download, DOWNLOAD_EVT_ERROR, err);
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

K_THREAD_DEFINE(download_module_thread, CONFIG_DOWNLOAD_THREAD_STACK_SIZE,
                module_thread_fn, NULL, NULL, NULL,
                K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);

EVENT_LISTENER(MODULE, event_handler);
EVENT_SUBSCRIBE(MODULE, cloud_module_event);