/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr.h>
#include <event_manager.h>
#include <net/aws_iot.h>
#include <modem/lte_lc.h>
#include <modem/nrf_modem_lib.h>
#include <modem/at_cmd.h>
#include <modem/at_notif.h>
#include <modem/modem_info.h>
#include <nrf_modem.h>
#include <cJSON.h>
#include <cJSON_os.h>

#include "events/cloud_module_event.h"

#define MODULE cloud_module

#include "modules_common.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_CLOUD_MODULE_LOG_LEVEL);

struct cloud_msg_data
{
	union
	{
		struct cloud_module_event cloud;
	} module;
};

/* Cloud module super states. */
static enum state_type { STATE_LTE_DISCONNECTED,
						 STATE_LTE_CONNECTED,
						 STATE_SHUTDOWN
} state;

/* Cloud module sub states. */
static enum sub_state_type { SUB_STATE_CLOUD_DISCONNECTED,
							 SUB_STATE_CLOUD_CONNECTED
} sub_state;

/* Keeps track of connection retries.*/
static int connection_retries = 0;

/* Cloud module message queue. */
#define CLOUD_QUEUE_ENTRY_COUNT 10
#define CLOUD_QUEUE_BYTE_ALIGNMENT 4

K_MSGQ_DEFINE(msgq_cloud, sizeof(struct cloud_msg_data),
			  CLOUD_QUEUE_ENTRY_COUNT, CLOUD_QUEUE_BYTE_ALIGNMENT);

/* Work item used to check if a cloud connection is established. */
static struct k_work_delayable connect_check_work;

static struct module_data self = {
	.name = "cloud",
	.msg_q = &msgq_cloud,
	.supports_shutdown = true,
};

/* Convenience functions used in internal state handling. */
static char *state2str(enum state_type state)
{
	switch (state)
	{
	case STATE_LTE_DISCONNECTED:
		return "STATE_LTE_DISCONNECTED";
	case STATE_LTE_CONNECTED:
		return "STATE_LTE_CONNECTED";
	case STATE_SHUTDOWN:
		return "STATE_SHUTDOWN";
	default:
		return "Unknown";
	}
}

static char *sub_state2str(enum sub_state_type new_state)
{
	switch (new_state)
	{
	case SUB_STATE_CLOUD_DISCONNECTED:
		return "SUB_STATE_CLOUD_DISCONNECTED";
	case SUB_STATE_CLOUD_CONNECTED:
		return "SUB_STATE_CLOUD_CONNECTED";
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

	LOG_DBG("State transition %s --> %s",
			state2str(state),
			state2str(new_state));

	state = new_state;
}

static void sub_state_set(enum sub_state_type new_state)
{
	if (new_state == sub_state)
	{
		LOG_DBG("Sub state: %s", sub_state2str(sub_state));
		return;
	}

	LOG_DBG("Sub state transition %s --> %s",
			sub_state2str(sub_state),
			sub_state2str(new_state));

	sub_state = new_state;
}

static void connect_aws(void)
{
	int err;
	int backoff_sec = 32;

	if (connection_retries > CONFIG_CLOUD_CONNECT_RETRIES)
	{
		LOG_WRN("Too many failed connection attempts");
		SEND_ERROR(cloud, CLOUD_EVT_ERROR, -ENETUNREACH);
		return;
	}

	LOG_DBG("Connecting to AWS");
	err = aws_iot_connect(NULL);
	if (err < 0)
	{
		LOG_ERR("AWS connection failed, error: %d", err);
	}

	connection_retries++;
	LOG_WRN("Establishing connection. Retry in %d seconds if not successful.", backoff_sec);
	k_work_reschedule(&connect_check_work, K_SECONDS(backoff_sec));
}

/* If this work is executed, it means that the connection attempt was not
 * successful before the backoff timer expired. A timeout message is then
 * added to the message queue to signal the timeout.
 */
static void connect_check_work_fn(struct k_work *work)
{
	// If cancelling works fails
	if ((state == STATE_LTE_CONNECTED && sub_state == SUB_STATE_CLOUD_CONNECTED) ||
		(state == STATE_LTE_DISCONNECTED))
	{
		return;
	}

	LOG_DBG("Cloud connection timeout occurred");

	SEND_EVENT(cloud, CLOUD_EVT_CONNECTION_TIMEOUT);
}

static void aws_iot_evt_handler(const struct aws_iot_evt *event)
{
	switch (event->type)
	{
	case AWS_IOT_EVT_CONNECTING:
	{
		LOG_DBG("Connecting to AWS");
		SEND_EVENT(cloud, CLOUD_EVT_CONNECTING);
		break;
	}
	case AWS_IOT_EVT_CONNECTED:
	{
		LOG_DBG("Connected to AWS");
		sub_state_set(SUB_STATE_CLOUD_CONNECTED);
		break;
	}
	case AWS_IOT_EVT_DISCONNECTED:
	{
		LOG_DBG("Disconnected from AWS");
		sub_state_set(SUB_STATE_CLOUD_DISCONNECTED);
		break;
	}
	case AWS_IOT_EVT_DATA_RECEIVED: {
		LOG_DBG("Data recieved on topic %s",log_strdup(event->data.msg.topic.str));
		event->data.msg.ptr[event->data.msg.len] = 0;
		LOG_DBG("Message %s",log_strdup(event->data.msg.ptr));
		break;
	}
	default:
		LOG_WRN("Unknown AWS event type %d", event->type);
		break;
	}
	return;
}

/* Message handler for STATE_LTE_CONNECTED. */
static void on_state_lte_connected(struct cloud_msg_data *msg)
{
	if (IS_EVENT(msg, cloud, CLOUD_EVT_LTE_DISCONNECTED))
	{
		state_set(STATE_LTE_DISCONNECTED);
		sub_state_set(SUB_STATE_CLOUD_DISCONNECTED);

		aws_iot_disconnect();

		connection_retries = 0;

		k_work_cancel_delayable(&connect_check_work);

		return;
	}
}

/* Message handler for STATE_LTE_DISCONNECTED. */
static void on_state_lte_disconnected(struct cloud_msg_data *msg)
{
	if (IS_EVENT(msg, cloud, CLOUD_EVT_LTE_CONNECTED))
	{
		state_set(STATE_LTE_CONNECTED);

		/* LTE is now connected, cloud connection can be attempted */
		connect_aws();
	}
}

/* Message handler for SUB_STATE_CLOUD_CONNECTED. */
static void on_sub_state_cloud_connected(struct cloud_msg_data *msg)
{
	if (IS_EVENT(msg, cloud, CLOUD_EVT_DISCONNECTED))
	{
		sub_state_set(SUB_STATE_CLOUD_DISCONNECTED);

		k_work_reschedule(&connect_check_work, K_NO_WAIT);

		return;
	}
}

/* Message handler for SUB_STATE_CLOUD_DISCONNECTED. */
static void on_sub_state_cloud_disconnected(struct cloud_msg_data *msg)
{
	if (IS_EVENT(msg, cloud, CLOUD_EVT_CONNECTED))
	{
		sub_state_set(SUB_STATE_CLOUD_CONNECTED);

		connection_retries = 0;
		k_work_cancel_delayable(&connect_check_work);
	}

	if (IS_EVENT(msg, cloud, CLOUD_EVT_CONNECTION_TIMEOUT))
	{
		connect_aws();
	}
}

/* Message handler for all states. */
static void on_all_states(struct cloud_msg_data *msg)
{
	//TODO: Implement or remove
	return;
}

static void lte_handler(const struct lte_lc_evt *evt)
{
	switch (evt->type)
	{
	case LTE_LC_EVT_NW_REG_STATUS:
		if ((evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_HOME) &&
			(evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_ROAMING))
		{
			SEND_EVENT(cloud, CLOUD_EVT_LTE_DISCONNECTED);
			break;
		}

		LOG_DBG("Network registration status: %s",
				evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME ? "Connected - home network" : "Connected - roaming");

		SEND_EVENT(cloud, CLOUD_EVT_LTE_CONNECTED);
		break;
	case LTE_LC_EVT_PSM_UPDATE:
		LOG_DBG("PSM parameter update: TAU: %d, Active time: %d",
				evt->psm_cfg.tau, evt->psm_cfg.active_time);
		break;
	case LTE_LC_EVT_EDRX_UPDATE:
	{
		char log_buf[60];
		ssize_t len;

		len = snprintf(log_buf, sizeof(log_buf),
					   "eDRX parameter update: eDRX: %f, PTW: %f",
					   evt->edrx_cfg.edrx, evt->edrx_cfg.ptw);
		if (len > 0)
		{
			LOG_DBG("%s", log_buf);
		}
		break;
	}
	case LTE_LC_EVT_RRC_UPDATE:
		LOG_DBG("RRC mode: %s",
				evt->rrc_mode == LTE_LC_RRC_MODE_CONNECTED ? "Connected" : "Idle");
		break;
	case LTE_LC_EVT_CELL_UPDATE:
		LOG_DBG("LTE cell changed: Cell ID: %d, Tracking area: %d",
				evt->cell.id, evt->cell.tac);
		break;
	default:
		break;
	}
}

static int modem_configure(void)
{
	int err;

	err = lte_lc_init_and_connect_async(lte_handler);
	if (err)
	{
		LOG_ERR("Modem could not be configured, error: %d", err);
		return -ENETUNREACH;
	}

	return 0;
}

static int setup(void)
{
	int err;
	err = modem_configure();
	if (err)
	{
		LOG_ERR("modem_configure, error: %d", err);
		return err;
	}
	err = aws_iot_init(NULL, aws_iot_evt_handler);
	if (err)
	{
		LOG_ERR("aws_iot_init, error: %d", err);
		return err;
	}

	return 0;
}

static void module_thread_fn(void)
{
	LOG_DBG("Cloud module thread started");
	int err;
	struct cloud_msg_data msg;
	self.thread_id = k_current_get();

	err = module_start(&self);
	if (err)
	{
		LOG_ERR("Failed starting module, error: %d", err);
		SEND_ERROR(cloud, CLOUD_EVT_ERROR, err);
	}

	state_set(STATE_LTE_DISCONNECTED);
	sub_state_set(SUB_STATE_CLOUD_DISCONNECTED);

	k_work_init_delayable(&connect_check_work, connect_check_work_fn);

	err = setup();
	if (err)
	{
		LOG_ERR("setup, error %d", err);
		SEND_ERROR(cloud, CLOUD_EVT_ERROR, err);
	}

	while (true)
	{
		module_get_next_msg(&self, &msg, K_FOREVER);

		switch (state)
		{
		case STATE_LTE_CONNECTED:
			switch (sub_state)
			{
			case SUB_STATE_CLOUD_CONNECTED:
				on_sub_state_cloud_connected(&msg);
				break;
			case SUB_STATE_CLOUD_DISCONNECTED:
				on_sub_state_cloud_disconnected(&msg);
				break;
			default:
				LOG_ERR("Unknown Cloud module sub state");
				break;
			}
			on_state_lte_connected(&msg);
			break;
		case STATE_LTE_DISCONNECTED:
			on_state_lte_disconnected(&msg);
			break;
		case STATE_SHUTDOWN:
			/* The shutdown state has no transition. */
			break;
		default:
			LOG_ERR("Unknown Cloud module state.");
			break;
		}

		on_all_states(&msg);
	}
}

/* Handles Event Manager events */
static bool event_handler(const struct event_header *eh)
{
	struct cloud_msg_data msg = {0};
	bool enqueue_msg = false;

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
			SEND_ERROR(cloud, CLOUD_EVT_ERROR, err);
		}
	}

	return false;
}

K_THREAD_DEFINE(cloud_module_thread, CONFIG_CLOUD_THREAD_STACK_SIZE,
				module_thread_fn, NULL, NULL, NULL,
				K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);

EVENT_LISTENER(MODULE, event_handler);
EVENT_SUBSCRIBE(MODULE, cloud_module_event);