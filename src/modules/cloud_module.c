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
#include <date_time.h>
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
						 STATE_SHUTDOWN,
} state;

/* Cloud module sub states. */
static enum sub_state_type { SUB_STATE_CLOUD_DISCONNECTED,
							 SUB_STATE_CLOUD_CONNECTED,
} sub_state;

/* Keeps track of connection retries.*/
static int connection_retries = 0;

/* Cloud module message queue. */
#define CLOUD_QUEUE_ENTRY_COUNT 10
#define CLOUD_QUEUE_BYTE_ALIGNMENT 4

K_MSGQ_DEFINE(msgq_cloud, sizeof(struct cloud_msg_data),
			  CLOUD_QUEUE_ENTRY_COUNT, CLOUD_QUEUE_BYTE_ALIGNMENT);

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

//========================================================================================
/*                                                                                      *
 *                               AWS related functionality                              *
 *                                                                                      */
//========================================================================================

// TODO: Find a better way to check for topics + remove hardcoded ID.
#define AWS_IOT_TOPIC_SHADOW_UPDATE_DELTA "$aws/things/352656109498066/shadow/update/delta"

/**
 * @brief State of device as reported to the cloud service. 
 * Should match the expected format. 
 */
struct device_state
{
	int value;
};

/**
 * @brief Work item used to check if a cloud connection is established.
 */
static struct k_work_delayable connect_check_work;

/**
 * @brief Initialize connection to AWS
 * 
 */
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

/**
 * @brief Convenience macro for deleting root and returning an error on NULL.
 */
#define NOT_NULL(expression, root, error) \
	if (expression == NULL)               \
	{                                     \
		cJSON_Delete(root);               \
		return -ENOMEM;                   \
	}

/**
 * @brief Updates AWS device shadow.
 * 
 * @param delta Desired state delta.
 * @param timestamp Timestamp of delta. 
 * @return 0 on success, negative errno on error.
 */
static int update_shadow(struct device_state delta, uint64_t timestamp)
{
	static uint64_t last_handled_shadow = 0; // Timestamp of last accepted shadow.
	if (timestamp < last_handled_shadow)
	{
		// More recent shadow received. Do nothing.
		return 0;
	}
	last_handled_shadow = timestamp;

	cJSON *root = cJSON_CreateObject();
	NOT_NULL(root, root, -ENOMEM);
	cJSON *state = cJSON_AddObjectToObject(root, "state");
	NOT_NULL(state, root, -ENOMEM);
	cJSON *reported = cJSON_AddObjectToObject(state, "reported");
	NOT_NULL(reported, root, -ENOMEM);

	int64_t msg_ts = 0; // Timestamp for shadow update.
	int16_t batv = 0;	// Battery voltage

	if (!date_time_now(&msg_ts))
	{
		NOT_NULL(cJSON_AddNumberToObject(reported, "ts", msg_ts), root, -ENOMEM);
	}
	else
		LOG_WRN("Could not get timestamp.");

	if (modem_info_short_get(MODEM_INFO_BATTERY, &batv) == sizeof(batv))
	{
		NOT_NULL(cJSON_AddNumberToObject(reported, "batv", batv), root, -ENOMEM);
	}
	else
		LOG_WRN("Could not get battery voltage.");

	NOT_NULL(cJSON_AddNumberToObject(reported, "value", delta.value), root, -ENOMEM);

	char *message = cJSON_Print(root);
	NOT_NULL(message, root, -ENOMEM);

	struct aws_iot_data tx_data = {
		.qos = MQTT_QOS_0_AT_MOST_ONCE,
		.topic.type = AWS_IOT_SHADOW_TOPIC_UPDATE,
		.ptr = message,
		.len = strlen(message)};

	LOG_DBG("Updating shadow");
	int err = aws_iot_send(&tx_data);

	cJSON_free(message);
	cJSON_Delete(root);

	return err;
}
#undef NOT_NULL

static void handle_cloud_data(const struct aws_iot_data *msg)
{
	if (!strcmp(msg->topic.str, AWS_IOT_TOPIC_SHADOW_UPDATE_DELTA))
	{
		cJSON *json = cJSON_Parse(msg->ptr);
		cJSON *state = cJSON_GetObjectItemCaseSensitive(json, "state");
		cJSON *value = cJSON_GetObjectItemCaseSensitive(state, "value");
		cJSON *timestamp_obj = cJSON_GetObjectItemCaseSensitive(json, "timestamp");
		struct device_state delta;
		int64_t timestamp = timestamp_obj->valueint;
		delta.value = value->valueint;
		cJSON_Delete(json);
		update_shadow(delta, timestamp);
	}
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

//========================================================================================
/*                                                                                      *
 *                                    State handlers/                                   *
 *                                 transition functions                                 *
 *                                                                                      */
//========================================================================================

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

/**
 * @brief LTE event handler
 * 
 * @param evt Event
 */
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

/**
 * @brief AWS iot event handler.
 * 
 * @param evt Event
 */
static void aws_iot_evt_handler(const struct aws_iot_evt *evt)
{
	switch (evt->type)
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
	case AWS_IOT_EVT_READY:
	{
		LOG_DBG("AWS ready");
		break;
	}
	case AWS_IOT_EVT_DATA_RECEIVED:
	{
		LOG_DBG("Data recieved on topic %s", log_strdup(evt->data.msg.topic.str));
		LOG_DBG("Message %s", log_strdup(evt->data.msg.ptr));
		handle_cloud_data(&evt->data.msg);
		break;
	}
	default:
		LOG_WRN("Unknown AWS event type %d", evt->type);
		break;
	}
	return;
}

//========================================================================================
/*                                                                                      *
 *                                  Setup/configuration                                 *
 *                                                                                      */
//========================================================================================

/**
 * @brief Configures modem and initializes LTE connection.
 * 
 * @return int 0 on success, negative errno code on failure.
 */
static int modem_configure(void)
{
	int err;
	state_set(STATE_LTE_DISCONNECTED);
	err = lte_lc_init_and_connect_async(lte_handler);
	if (err)
	{
		LOG_ERR("Modem could not be configured, error: %d", err);
		return err;
	}
	err = modem_info_init();
	if (err) {
		LOG_ERR("Modem info could not be initialized: %d", err);
		return err;
	}


	return 0;
}

/**
 * @brief Initializes cloud related resources.
 * 
 * @return int 0 on success, negative errno code on failure.
 */
static int cloud_configure(void)
{
	sub_state_set(SUB_STATE_CLOUD_DISCONNECTED);
	int err = aws_iot_init(NULL, aws_iot_evt_handler);
	if (err)
	{
		LOG_ERR("aws_iot_init, error: %d", err);
		return err;
	}
	k_work_init_delayable(&connect_check_work, connect_check_work_fn);
	return 0;
}

/**
 * @brief Setup function for the module. Initializes modem and AWS connection.
 * 
 * @return int 0 on success, negative errno code on failure.
 */
static int setup(void)
{
	int err;
	err = modem_configure();
	if (err)
	{
		LOG_ERR("modem_configure, error: %d", err);
		return err;
	}
	err = cloud_configure();
	if (err)
	{
		LOG_ERR("cloud_configure, error: %d", err);
		return err;
	}

	return 0;
}

//========================================================================================
/*                                                                                      *
 *                                     Module thread                                    *
 *                                                                                      */
//========================================================================================

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

K_THREAD_DEFINE(cloud_module_thread, CONFIG_CLOUD_THREAD_STACK_SIZE,
				module_thread_fn, NULL, NULL, NULL,
				K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);

EVENT_LISTENER(MODULE, event_handler);
EVENT_SUBSCRIBE(MODULE, cloud_module_event);