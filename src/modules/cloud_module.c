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
#include <string.h>
#include <cJSON.h>
#include <cJSON_os.h>

#include "events/cloud_module_event.h"
#include "events/download_module_event.h"
#include "events/modem_module_event.h"
#include "util/cjson_util.h"

#define MODULE cloud_module

#include "modules_common.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_CLOUD_MODULE_LOG_LEVEL);

struct cloud_msg_data
{
	union
	{
		struct cloud_module_event cloud;
		struct download_module_event download;
		struct modem_module_event modem;
	} module;
};

/* Cloud module super states. */
static enum state_type { STATE_LTE_DISCONNECTED,
						 STATE_LTE_CONNECTED,
						 STATE_SHUTDOWN,
} state;

/* Cloud module sub states. */
static enum cloud_state_type { CLOUD_STATE_CLOUD_DISCONNECTED,
							   CLOUD_STATE_CLOUD_CONNECTED,
} cloud_state;

/**
 * @brief Work item used to check if a cloud connection is established.
 */
static struct k_work_delayable connect_check_work;

struct cloud_backoff_delay_lookup
{
	int delay;
};

/* Lookup table for backoff reconnection to cloud. Binary scaling. */
static struct cloud_backoff_delay_lookup backoff_delay[] = {
	{32}, {64}, {128}, {256}, {512}, {2048}, {4096}, {8192}, {16384}, {32768}, {65536}, {131072}, {262144}, {524288}, {1048576}};

/* Variable that keeps track of how many times a reconnection to cloud
 * has been tried without success.
 */
static int connect_retries;

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

/* Forward declarations. */
static void connect_check_work_fn(struct k_work *work);
static void send_config_received(void);

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

static char *cloud_state2str(enum cloud_state_type new_state)
{
	switch (new_state)
	{
	case CLOUD_STATE_CLOUD_DISCONNECTED:
		return "CLOUD_STATE_CLOUD_DISCONNECTED";
	case CLOUD_STATE_CLOUD_CONNECTED:
		return "CLOUD_STATE_CLOUD_CONNECTED";
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

static void cloud_state_set(enum cloud_state_type new_state)
{
	if (new_state == cloud_state)
	{
		LOG_DBG("Cloud state: %s", cloud_state2str(cloud_state));
		return;
	}

	LOG_DBG("Cloud state transition %s --> %s",
			cloud_state2str(cloud_state),
			cloud_state2str(new_state));

	cloud_state = new_state;
}

//========================================================================================
/*                                                                                      *
 *                               AWS related functionality                              *
 *                                                                                      */
//========================================================================================

#if !defined(CONFIG_CLOUD_CLIENT_ID_USE_CUSTOM)
#define AWS_CLOUD_CLIENT_ID_LEN 15
#else
#define AWS_CLOUD_CLIENT_ID_LEN (sizeof(CONFIG_CLOUD_CLIENT_ID) - 1)
#endif

#define AWS "$aws/things/"
#define AWS_LEN (sizeof(AWS) - 1)
#define UPDATE_DELTA_TOPIC AWS "%s/shadow/update/delta"
#define UPDATE_DELTA_TOPIC_LEN (AWS_LEN + AWS_CLOUD_CLIENT_ID_LEN + 20)
#define GET_ACCEPTED_TOPIC AWS "%s/shadow/get/accepted"
#define GET_ACCEPTED_TOPIC_LEN (AWS_LEN + AWS_CLOUD_CLIENT_ID_LEN + 20)

#define APP_SUB_TOPICS_COUNT 1
#define APP_PUB_TOPICS_COUNT 2

#define REQUEST_SHADOW_DOCUMENT_STRING ""

static char client_id_buf[AWS_CLOUD_CLIENT_ID_LEN + 1];
static char update_delta_topic[UPDATE_DELTA_TOPIC_LEN + 1];
static char get_accepted_topic[GET_ACCEPTED_TOPIC_LEN + 1];

static struct aws_iot_config config;

#define STATUS_UPDATE_WAIT_TIME_S (5)
// Pointer to currently active shadow response object. Should only be accessed after aquiring shadow_response_mutex.
static cJSON *shadow_response_root;
static K_MUTEX_DEFINE(shadow_response_mutex);

static void lock_shadow_response(void)
{
	k_mutex_lock(&shadow_response_mutex, K_FOREVER);
}

static void release_shadow_response(void)
{
	k_mutex_unlock(&shadow_response_mutex);
}
	

static int populate_app_endpoint_topics(void)
{
	int err;

	// 	err = snprintf(batch_topic, sizeof(batch_topic), BATCH_TOPIC,
	// 				   client_id_buf);
	// 	if (err != BATCH_TOPIC_LEN)
	// 	{
	// 		return -ENOMEM;
	// 	}

	// 	pub_topics[0].str = batch_topic;
	// 	pub_topics[0].len = BATCH_TOPIC_LEN;

	// 	err = snprintf(messages_topic, sizeof(messages_topic), MESSAGES_TOPIC,
	// 				   client_id_buf);
	// 	if (err != MESSAGES_TOPIC_LEN)
	// 	{
	// 		return -ENOMEM;
	// 	}

	// 	pub_topics[1].str = messages_topic;
	// 	pub_topics[1].len = MESSAGES_TOPIC_LEN;

	// 	err = snprintf(cfg_topic, sizeof(cfg_topic), CFG_TOPIC, client_id_buf);
	// 	if (err != CFG_TOPIC_LEN)
	// 	{
	// 		return -ENOMEM;
	// 	}

	// 	sub_topics[0].str = cfg_topic;
	// 	sub_topics[0].len = CFG_TOPIC_LEN;

	// err = snprintf(update_delta_topic, sizeof(update_delta_topic), UPDATE_DELTA_TOPIC,
	// 			   client_id_buf);
	// if (err != UPDATE_DELTA_TOPIC_LEN)
	// {
	// 	return -ENOMEM;
	// }

	// err = snprintf(get_accepted_topic, sizeof(get_accepted_topic), GET_ACCEPTED_TOPIC,
	// 			   client_id_buf);
	// if (err != GET_ACCEPTED_TOPIC_LEN)
	// {
	// 	return -ENOMEM;
	// }

	// 	err = aws_iot_subscription_topics_add(sub_topics,
	// 										  ARRAY_SIZE(sub_topics));
	// 	if (err)
	// 	{
	// 		LOG_ERR("cloud_ep_subscriptions_add, error: %d", err);
	// 		return err;
	// 	}

	return 0;
}

/**
 * @brief Initialize connection to AWS
 * 
 */
static void connect_aws(void)
{
	int err;
	int backoff_sec = backoff_delay[connect_retries].delay;

	if (connect_retries > CONFIG_CLOUD_CONNECT_RETRIES)
	{
		LOG_WRN("Too many failed connection attempts");
		SEND_ERROR(cloud, CLOUD_EVT_ERROR, -ENETUNREACH);
		return;
	}

	LOG_DBG("Connecting to AWS");
	err = aws_iot_connect(&config);
	if (err < 0)
	{
		LOG_ERR("AWS connection failed, error: %d", err);
	}

	connect_retries++;
	LOG_WRN("Establishing connection. Retry in %d seconds if not successful.", backoff_sec);
	k_work_reschedule(&connect_check_work, K_SECONDS(backoff_sec));
}

static void submit_shadow_update_work_fn(struct k_work *work)
{
	lock_shadow_response();
	ARG_UNUSED(work);
	LOG_DBG("Submiting shadow updates");
	char *message = cJSON_PrintUnformatted(shadow_response_root);
	struct aws_iot_data tx_data = {
		.qos = MQTT_QOS_0_AT_MOST_ONCE,
		.topic.type = AWS_IOT_SHADOW_TOPIC_UPDATE,
		.ptr = message,
		.len = strlen(message),
	};
	aws_iot_send(&tx_data);
	cJSON_free(message);
	cJSON_Delete(shadow_response_root);
	shadow_response_root = cJSON_CreateObject();
	release_shadow_response();
}

// Work item used to submit the accumulated shadow update.
static K_WORK_DELAYABLE_DEFINE(submit_shadow_update_work, submit_shadow_update_work_fn);

/**
 * @brief Informs the cloud that the password download failed.
 */
static void handle_password_download_failed(void)
{
	lock_shadow_response();
	cJSON *root = shadow_response_root;
	cJSON *state_obj = cJSON_GetOrAddObjectItemCS(root, "state");
	cJSON *reported_obj = cJSON_GetOrAddObjectItemCS(state_obj, "reported");
	cJSON *skykey_obj = cJSON_GetOrAddObjectItemCS(reported_obj, "skyKey");
	cJSON_AddStringToObjectCS(skykey_obj, "databaseDownloadStatus", "failed");

	k_work_reschedule(&submit_shadow_update_work, K_SECONDS(STATUS_UPDATE_WAIT_TIME_S ));
	release_shadow_response();
}

/**
 * @brief Informs the cloud that the password download is complete.
 */
static void handle_password_download_complete(void)
{
	lock_shadow_response();
	cJSON *root = shadow_response_root;
	cJSON *state_obj = cJSON_GetOrAddObjectItemCS(root, "state");
	cJSON *reported_obj = cJSON_GetOrAddObjectItemCS(state_obj, "reported");
	cJSON *skykey_obj = cJSON_GetOrAddObjectItemCS(reported_obj, "skyKey");
	cJSON_AddStringToObjectCS(skykey_obj, "databaseDownloadStatus", "complete");

	k_work_reschedule(&submit_shadow_update_work, K_SECONDS(STATUS_UPDATE_WAIT_TIME_S ));
	release_shadow_response();
}

/**
 * @brief Type for functions that handle a shadow delta and initializes a response modification.
 * 
 * @param delta Shadow delta root. Should not be modified.
 * @return 0 on success, negative errno otherwise. Non-essential handlers should always return 0.
 */
typedef int (*shadow_delta_handler_t)(cJSON *delta);

/**
 * @brief Handles password database related deltas.	
 * 
 * @param delta Shadow delta root. Should not be modified.
 * @return 0 on success, negative errno otherwise.
 */
static int handle_password_delta(cJSON *delta)
{
	cJSON *skykey_delta = cJSON_GetObjectItemCaseSensitive(delta, "skyKey");
	if (skykey_delta == NULL || !cJSON_IsObject(skykey_delta))
	{
		return 0;
	}
	cJSON *password_delta = cJSON_GetObjectItemCaseSensitive(skykey_delta, "databaseLocation");
	if (password_delta != NULL && password_delta->type == cJSON_String)
	{
		struct cloud_module_event *evt = new_cloud_module_event();
		evt->type = CLOUD_EVT_DATABASE_UPDATE_AVAILABLE;
		strncpy(evt->data.url, password_delta->valuestring, sizeof(evt->data.url));
		evt->data.url[sizeof(evt->data.url) - 1] = '\0'; // Ensure null termination
		EVENT_SUBMIT(evt);
		lock_shadow_response();
		cJSON *root = shadow_response_root;
		cJSON *state_obj = cJSON_GetOrAddObjectItemCS(root, "state");
		cJSON *reported_obj = cJSON_GetOrAddObjectItemCS(state_obj, "reported");
		cJSON *skykey_obj = cJSON_GetOrAddObjectItemCS(reported_obj, "skyKey");
		cJSON_AddStringToObjectCS(skykey_obj, "databaseLocation", password_delta->valuestring);
		k_work_reschedule(&submit_shadow_update_work, K_SECONDS(STATUS_UPDATE_WAIT_TIME_S ));
		release_shadow_response();
	}
	return 0;
}

static int handle_lock_timeout_delta(cJSON *delta)
{
	cJSON *skykey_delta = cJSON_GetObjectItemCaseSensitive(delta, "skyKey");
	char *lock_timeout_prop_name = "lockTimeoutSeconds";
	if (skykey_delta == NULL || !cJSON_IsObject(skykey_delta))
	{
		return 0;
	}
	cJSON *lock_timeout_delta = cJSON_GetObjectItemCaseSensitive(skykey_delta, lock_timeout_prop_name);
	if (lock_timeout_delta != NULL && lock_timeout_delta->type == cJSON_Number)
	{
		struct cloud_module_event *evt = new_cloud_module_event();
		evt->type = CLOUD_EVT_NEW_LOCK_TIMEOUT;
		evt->data.timeout = lock_timeout_delta->valueint;
		EVENT_SUBMIT(evt);
		// TODO: Get confirmation from the lock module. This should be done in a separate function.
		// HACK: For now we just assume the lock module accepted the timeout.
		lock_shadow_response();
		cJSON *root = shadow_response_root;
		cJSON *state_obj = cJSON_GetOrAddObjectItemCS(root, "state");
		cJSON *reported_obj = cJSON_GetOrAddObjectItemCS(state_obj, "reported");
		cJSON *skykey_obj = cJSON_GetOrAddObjectItemCS(reported_obj, "skyKey");
		cJSON_AddNumberToObjectCS(skykey_obj, lock_timeout_prop_name, lock_timeout_delta->valueint);
		release_shadow_response();
	}
	return 0;
}

/**
 * @brief Adds some basic device information to the shadow response.
 * Might do more things in the future.
 * 
 * @param delta UNUSED
 * @return 0 on success, negative errno otherwise.
 */
static int add_device_status(cJSON *delta)
{
	ARG_UNUSED(delta);
	lock_shadow_response();
	cJSON *root = shadow_response_root;
	cJSON *state_obj = cJSON_GetOrAddObjectItemCS(root, "state");
	cJSON *reported_obj = cJSON_GetOrAddObjectItemCS(state_obj, "reported");
	cJSON *dev_obj = cJSON_GetOrAddObjectItemCS(reported_obj, "dev");
	cJSON *version_obj = cJSON_GetOrAddObjectItemCS(dev_obj, "v");
	cJSON_AddStringToObjectCS(version_obj, "appV", CONFIG_SKYKEY_FW_VERSION);
	cJSON_AddStringToObjectCS(version_obj, "brdV", CONFIG_SKYKEY_BOARD_VERSION);
	release_shadow_response();
	return 0;
}

/**
 * @brief Delta handler pipeline. Functions here will get called in order with the delta and response as arguments.
 * Functions should return 0 on success, negative errno otherwise.
 */
static shadow_delta_handler_t shadow_delta_handlers[] = {
	handle_password_delta,
	handle_lock_timeout_delta,
	add_device_status,
};

/**
 * @brief Updates AWS device shadow.
 * 
 * @param delta Desired state delta.
 * @param timestamp Timestamp of delta. 
 * @return 0 on success, negative errno on error.
 */
static int update_shadow(cJSON *delta, int64_t timestamp)
{
	static uint64_t last_handled_shadow = 0; // Timestamp of last accepted shadow.
	if (timestamp < last_handled_shadow)
	{
		// More recent shadow received. Do nothing.
		return 0;
	}
	lock_shadow_response();
	cJSON *root = shadow_response_root;
	cJSON *state_obj = cJSON_GetOrAddObjectItemCS(root, "state");
	cJSON *reported_obj = cJSON_GetOrAddObjectItemCS(state_obj, "reported");
	cJSON *dev_obj = cJSON_GetOrAddObjectItemCS(reported_obj, "dev");
	cJSON_AddNumberToObjectCS(dev_obj, "ts", timestamp);
	release_shadow_response();

	last_handled_shadow = timestamp;

	for (int i = 0; i < sizeof(shadow_delta_handlers) / sizeof(shadow_delta_handlers[0]); i++)
	{
		int ret = shadow_delta_handlers[i](delta);
		if (ret < 0)
		{
			LOG_WRN("Delta handler with index %d returned %d", i, ret);
			return ret;
		}
	}
	k_work_reschedule(&submit_shadow_update_work, K_SECONDS(STATUS_UPDATE_WAIT_TIME_S ));
	return 0;
}

static void handle_cloud_data(const struct aws_iot_data *msg)
{
	if (!strcmp(msg->topic.str, get_accepted_topic) || !strcmp(msg->topic.str, update_delta_topic))
	{
		cJSON *json = cJSON_Parse(msg->ptr);
		cJSON *state = cJSON_GetObjectItemCaseSensitive(json, "state");
		cJSON *timestamp_obj = cJSON_GetObjectItemCaseSensitive(json, "timestamp");
		int64_t timestamp = timestamp_obj->valueint;

		update_shadow(state, timestamp);
		cJSON_Delete(json);
	}
}

/* If this work is executed, it means that the connection attempt was not
 * successful before the backoff timer expired. A timeout message is then
 * added to the message queue to signal the timeout.
 */
static void connect_check_work_fn(struct k_work *work)
{
	// If cancelling works fails
	if ((state == STATE_LTE_CONNECTED && cloud_state == CLOUD_STATE_CLOUD_CONNECTED) ||
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
	if (IS_EVENT(msg, modem, MODEM_EVT_LTE_DISCONNECTED))
	{
		state_set(STATE_LTE_DISCONNECTED);
		cloud_state_set(CLOUD_STATE_CLOUD_DISCONNECTED);

		aws_iot_disconnect();

		connect_retries = 0;

		k_work_cancel_delayable(&connect_check_work);

		return;
	}
}

/* Message handler for STATE_LTE_DISCONNECTED. */
static void on_state_lte_disconnected(struct cloud_msg_data *msg)
{
	if (IS_EVENT(msg, modem, MODEM_EVT_LTE_CONNECTED))
	{
		state_set(STATE_LTE_CONNECTED);
		/* Update current time. */
		date_time_update_async(NULL);

		/* LTE is now connected, cloud connection can be attempted */
		connect_aws();
	}
}

/* Message handler for CLOUD_STATE_CLOUD_CONNECTED. */
static void on_cloud_state_cloud_connected(struct cloud_msg_data *msg)
{
	if (IS_EVENT(msg, cloud, CLOUD_EVT_DISCONNECTED))
	{
		cloud_state_set(CLOUD_STATE_CLOUD_DISCONNECTED);

		k_work_reschedule(&connect_check_work, K_NO_WAIT);

		return;
	}

	if (IS_EVENT(msg, download, DOWNLOAD_EVT_ERROR))
	{
		handle_password_download_failed();
	}

	if (IS_EVENT(msg, download, DOWNLOAD_EVT_DOWNLOAD_FINISHED))
	{
		handle_password_download_complete();
	}
}

/* Message handler for CLOUD_STATE_CLOUD_DISCONNECTED. */
static void on_cloud_state_cloud_disconnected(struct cloud_msg_data *msg)
{
	if (IS_EVENT(msg, cloud, CLOUD_EVT_CONNECTED))
	{
		cloud_state_set(CLOUD_STATE_CLOUD_CONNECTED);

		connect_retries = 0;
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
	//TODO: Implement this
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
	struct cloud_msg_data msg = {0};
	bool enqueue_msg = false;

	if (is_cloud_module_event(eh))
	{
		LOG_DBG("Cloud event recieved.");
		struct cloud_module_event *evt = cast_cloud_module_event(eh);
		msg.module.cloud = *evt;
		enqueue_msg = true;
	}

	if (is_download_module_event(eh))
	{
		struct download_module_event *evt = cast_download_module_event(eh);
		msg.module.download = *evt;
		enqueue_msg = true;
	}

	if (is_modem_module_event(eh))
	{
		struct modem_module_event *evt = cast_modem_module_event(eh);
		msg.module.modem = *evt;
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
		cloud_state_set(CLOUD_STATE_CLOUD_CONNECTED);
		break;
	}
	case AWS_IOT_EVT_DISCONNECTED:
	{
		LOG_DBG("Disconnected from AWS");
		cloud_state_set(CLOUD_STATE_CLOUD_DISCONNECTED);
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
 * @brief Initializes cloud related resources.
 * 
 * @return int 0 on success, negative errno code on failure.
 */
static int cloud_configure(void)
{
	cloud_state_set(CLOUD_STATE_CLOUD_DISCONNECTED);
	int err;

#if !defined(CONFIG_CLOUD_CLIENT_ID_USE_CUSTOM)
	char imei_buf[20];

	/* Retrieve device IMEI from modem. */
	err = at_cmd_write("AT+CGSN", imei_buf, sizeof(imei_buf), NULL);
	if (err)
	{
		LOG_ERR("Not able to retrieve device IMEI from modem");
		return err;
	}

	/* Set null character at the end of the device IMEI. */
	imei_buf[AWS_CLOUD_CLIENT_ID_LEN] = 0;

	strncpy(client_id_buf, imei_buf, sizeof(client_id_buf) - 1);

#else
	snprintf(client_id_buf, sizeof(client_id_buf), "%s",
			 CONFIG_CLOUD_CLIENT_ID);
#endif
	/* Fetch IMEI from modem data and set IMEI as cloud connection ID **/
	config.client_id = client_id_buf;
	config.client_id_len = strlen(client_id_buf);
	// LOG_DBG("Client ID: %s", client_id_buf);
	err = aws_iot_init(&config, aws_iot_evt_handler);
	if (err)
	{
		LOG_ERR("aws_iot_init, error: %d", err);
		return err;
	}

	/* Populate cloud specific endpoint topics */
	err = populate_app_endpoint_topics();
	if (err)
	{
		LOG_ERR("populate_app_endpoint_topics, error: %d", err);
		return err;
	}
	lock_shadow_response();
	shadow_response_root = cJSON_CreateObject();
	if (shadow_response_root == NULL)
	{
		return -ENOMEM;
	}
	release_shadow_response();
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
	err = cloud_configure();
	if (err)
	{
		LOG_ERR("cloud_configure, error: %d", err);
		return err;
	}

	err = snprintf(update_delta_topic, sizeof(update_delta_topic), UPDATE_DELTA_TOPIC,
				   client_id_buf);
	if (err != UPDATE_DELTA_TOPIC_LEN)
	{
		return -ENOMEM;
	}

	err = snprintf(get_accepted_topic, sizeof(get_accepted_topic), GET_ACCEPTED_TOPIC,
				   client_id_buf);
	if (err != GET_ACCEPTED_TOPIC_LEN)
	{
		return -ENOMEM;
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
			switch (cloud_state)
			{
			case CLOUD_STATE_CLOUD_CONNECTED:
				on_cloud_state_cloud_connected(&msg);
				break;
			case CLOUD_STATE_CLOUD_DISCONNECTED:
				on_cloud_state_cloud_disconnected(&msg);
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
EVENT_SUBSCRIBE(MODULE, download_module_event);
EVENT_SUBSCRIBE(MODULE, modem_module_event);