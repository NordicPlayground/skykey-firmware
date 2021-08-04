/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr.h>
#include <event_manager.h>
#include <string.h>

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
};

static enum { PASSWORD_STATE_DOWNLOADING,
              PASSWORD_STATE_FREE,
} module_state = PASSWORD_STATE_FREE;

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


static int handle_file_fragment(const void * const fragment, size_t frag_size, size_t file_size)
{
    // TODO: Actually do something usefull with the file fragment.
    static int frag_count = 0;
    static int data_recieved = 0;
    data_recieved += frag_size;
    int percentage = (data_recieved * 100) / file_size;
    LOG_DBG("Received fragment %d.\n Recieved: %d B/%d B\n  (%d%%)", frag_count++, data_recieved, file_size, percentage);
    return 0;
}

//========================================================================================
/*                                                                                      *
 *                                    State handlers/                                   *
 *                                 transition functions                                 *
 *                                                                                      */
//========================================================================================

static void on_state_free(struct password_msg_data *msg)
{
    if (IS_EVENT(msg, cloud, CLOUD_EVT_DATABASE_UPDATE_AVAILABLE))
    {

        LOG_WRN("%s", log_strdup(msg->module.cloud.data.url));
        struct cloud_module_event c_evt = msg->module.cloud;
        struct password_module_event *evt = new_password_module_event();
        evt->type = PASSWORD_EVT_REQ_DOWNLOAD;
        strcpy(evt->data.download_params.url, msg->module.cloud.data.url);
        evt->data.download_params.version = c_evt.data.download_params.version;
        evt->data.download_params.callback = handle_file_fragment;
        //strcpy(evt->dyndata.data, msg->data);
        EVENT_SUBMIT(evt);
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

    if (is_password_module_event(eh))
    {
        struct password_module_event *evt = cast_password_module_event(eh);

        msg.module.password = *evt;
        enqueue_msg = true;
    }

    if (is_cloud_module_event(eh))
    {
        struct cloud_module_event *evt = cast_cloud_module_event(eh);

        msg.module.cloud = *evt;
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
    int err;

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
        module_get_next_msg(&self, &msg, K_FOREVER);
        switch (module_state)
        {
        case PASSWORD_STATE_FREE:
        {
            on_state_free(&msg);
            break;
        }
        case PASSWORD_STATE_DOWNLOADING:
        {
            on_state_downloading(&msg);
            break;
        }
        default:
            LOG_ERR("Unknown state %d", module_state);
        }
        on_all_states(&msg);
        //k_free(msg.data);
    }
}

K_THREAD_DEFINE(password_module_thread, CONFIG_PASSWORD_THREAD_STACK_SIZE,
                module_thread_fn, NULL, NULL, NULL,
                K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);

EVENT_LISTENER(MODULE, event_handler);
EVENT_SUBSCRIBE(MODULE, password_module_event);
EVENT_SUBSCRIBE(MODULE, cloud_module_event);