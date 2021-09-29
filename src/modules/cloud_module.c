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
static enum sub_state_type { SUB_STATE_CLOUD_DISCONNECTED,
							 SUB_STATE_CLOUD_CONNECTED,
} sub_state;

/* Cloud module download states. */
static enum download_state_type { DOWNLOAD_STATE_FREE,
								  DOWNLOAD_STATE_DOWNLOADING,
} pwd_download_state;

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

static char *download_state2str(enum download_state_type new_state)
{
	switch (new_state)
	{
	case DOWNLOAD_STATE_FREE:
		return "DOWNLOAD_STATE_FREE";
	case DOWNLOAD_STATE_DOWNLOADING:
		return "DOWNLOAD_STATE_DOWNLOADING";
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

static void pwd_download_state_set(enum download_state_type new_state)
{
	if (new_state == pwd_download_state)
	{
		LOG_DBG("Password download state: %s", download_state2str(pwd_download_state));
		return;
	}

	LOG_DBG("Password download state transition %s --> %s",
			download_state2str(pwd_download_state),
			download_state2str(new_state));

	pwd_download_state = new_state;
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

/**
 * @brief Convenience macro for deleting root and returning an error on NULL.
 */
#define CJSON_EXIT(expression, root, error) \
	if (expression == NULL)                 \
	{                                       \
		cJSON_Delete(root);                 \
		return -ENOMEM;                     \
	}

/**
 * @brief Informs the cloud that the password download failed.
 */
static void handle_password_download_failed(void)
{
	cJSON *root = cJSON_CreateObject();
	cJSON *state_obj = cJSON_AddObjectToObject(root, "state");
	cJSON *reported_obj = cJSON_AddObjectToObject(state_obj, "reported");
	cJSON *skykey_obj = cJSON_AddObjectToObjectCS(reported_obj, "skyKey");
	cJSON *databaseLocation_obj = cJSON_AddStringToObjectCS(skykey_obj, "databaseDownloadStatus", "failed");
	char *message = cJSON_PrintUnformatted(root);
	struct aws_iot_data tx_data = {
		.qos = MQTT_QOS_0_AT_MOST_ONCE,
		.topic.type = AWS_IOT_SHADOW_TOPIC_UPDATE,
		.ptr = message,
		.len = strlen(message),
	};
	aws_iot_send(&tx_data);
	cJSON_free(message);
	cJSON_Delete(root);
	pwd_download_state_set(DOWNLOAD_STATE_FREE);
}

/**
 * @brief Informs the cloud that the password download is complete.
 */
static void handle_password_download_complete(void)
{
	cJSON *root = cJSON_CreateObject();
	cJSON *state_obj = cJSON_AddObjectToObject(root, "state");
	cJSON *reported_obj = cJSON_AddObjectToObject(state_obj, "reported");
	cJSON *skykey_obj = cJSON_AddObjectToObjectCS(reported_obj, "skyKey");
	cJSON *databaseLocation_obj = cJSON_AddStringToObjectCS(skykey_obj, "databaseDownloadStatus", "complete");
	char *message = cJSON_PrintUnformatted(root);
	struct aws_iot_data tx_data = {
		.qos = MQTT_QOS_0_AT_MOST_ONCE,
		.topic.type = AWS_IOT_SHADOW_TOPIC_UPDATE,
		.ptr = message,
		.len = strlen(message),
	};
	aws_iot_send(&tx_data);
	cJSON_free(message);
	cJSON_Delete(root);
	pwd_download_state_set(DOWNLOAD_STATE_FREE);
}

/**
 * @brief Type for functions that handle a shadow delta and modifies the response.
 * 
 * @param desired_delta Shadow delta root. Should not be modified.
 * @param response_root Response object. Any data written here will be reported to cloud.
 * @return 0 on success, negative errno otherwise. Non-essential handlers should always return 0.
 */
typedef int (*shadow_delta_handler_t)(cJSON *desired_delta, cJSON *response_root);

/**
 * @brief Handles password database related deltas.	
 * 
 * @param desired_delta Shadow delta root. Should not be modified.
 * @param response_root Response object. Any data written here will be reported to cloud.
 * @return 0 on success, negative errno otherwise.
 */
static int handle_password_delta(cJSON *desired_delta, cJSON *response_root)
{
	cJSON *skykey_delta = cJSON_GetObjectItemCaseSensitive(desired_delta, "skyKey");
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
		pwd_download_state_set(DOWNLOAD_STATE_DOWNLOADING);

		cJSON *skykey_response = cJSON_GetOrAddObjectItemCS(response_root, "skyKey");
		cJSON_AddStringToObjectCS(skykey_response, "databaseLocation", password_delta->valuestring);
		cJSON_AddStringToObjectCS(skykey_response, "databaseDownloadStatus", "started");
	}
	return 0;
}

static int handle_lock_timeout_delta(cJSON *desired_delta, cJSON *response_root)
{
	cJSON *skykey_delta = cJSON_GetObjectItemCaseSensitive(desired_delta, "skyKey");
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
		cJSON *skykey_response = cJSON_GetOrAddObjectItemCS(response_root, "skyKey");
		// TODO: Await event confirming update.
		cJSON_AddNumberToObjectCS(skykey_response, lock_timeout_prop_name, lock_timeout_delta->valueint);
	}
	return 0;
}

static int add_device_status(cJSON *desired_delta, cJSON *response_root)
{
	ARG_UNUSED(desired_delta);
	cJSON *device_obj = cJSON_GetOrAddObjectItemCS(response_root, "dev");
	cJSON *dev_version_obj = cJSON_GetOrAddObjectItemCS(device_obj, "v");
	cJSON_AddStringToObjectCS(dev_version_obj, "appV", CONFIG_SKYKEY_FW_VERSION);
	cJSON_AddStringToObjectCS(dev_version_obj, "brdV", CONFIG_SKYKEY_BOARD_VERSION);
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
	last_handled_shadow = timestamp;

	cJSON *root = cJSON_CreateObject();
	CJSON_EXIT(root, root, -ENOMEM);
	cJSON *state = cJSON_AddObjectToObject(root, "state");
	CJSON_EXIT(state, root, -ENOMEM);
	cJSON *reported = cJSON_AddObjectToObject(state, "reported");
	CJSON_EXIT(reported, root, -ENOMEM);

	int64_t msg_ts = 0; // Timestamp for shadow update.

	// TODO: Implement as delta handler
	if (!date_time_now(&msg_ts))
	{
		CJSON_EXIT(cJSON_AddNumberToObject(reported, "ts", msg_ts), root, -ENOMEM);
	}
	else
	{
		LOG_WRN("Could not get timestamp.");
	}

	for (int i = 0; i < sizeof(shadow_delta_handlers) / sizeof(shadow_delta_handlers[0]); i++)
	{
		int ret = shadow_delta_handlers[i](delta, reported);
		if (ret < 0)
		{
			LOG_WRN("Delta handler with index %d returned %d", i, ret);
			return ret;
		}
	}

	char *message = cJSON_PrintUnformatted(root);
	CJSON_EXIT(message, root, -ENOMEM);

	struct aws_iot_data tx_data = {
		.qos = MQTT_QOS_0_AT_MOST_ONCE,
		.topic.type = AWS_IOT_SHADOW_TOPIC_UPDATE,
		.ptr = message,
		.len = strlen(message)};

	LOG_DBG("Updating shadow");
	int err = aws_iot_send(&tx_data);

	cJSON_free(message);
	cJSON_Delete(root);
	err = 0;
	return err;
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
	if (IS_EVENT(msg, modem, MODEM_EVT_LTE_DISCONNECTED))
	{
		state_set(STATE_LTE_DISCONNECTED);
		sub_state_set(SUB_STATE_CLOUD_DISCONNECTED);

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

/* Message handler for SUB_STATE_CLOUD_CONNECTED. */
static void on_sub_state_cloud_connected(struct cloud_msg_data *msg)
{
	if (IS_EVENT(msg, cloud, CLOUD_EVT_DISCONNECTED))
	{
		sub_state_set(SUB_STATE_CLOUD_DISCONNECTED);

		k_work_reschedule(&connect_check_work, K_NO_WAIT);

		return;
	}

	if (IS_EVENT(msg, download, DOWNLOAD_EVT_ERROR))
	{
	}

	if (IS_EVENT(msg, download, DOWNLOAD_EVT_ERROR))
	{
	}
}

/* Message handler for SUB_STATE_CLOUD_DISCONNECTED. */
static void on_sub_state_cloud_disconnected(struct cloud_msg_data *msg)
{
	if (IS_EVENT(msg, cloud, CLOUD_EVT_CONNECTED))
	{
		sub_state_set(SUB_STATE_CLOUD_CONNECTED);

		connect_retries = 0;
		k_work_cancel_delayable(&connect_check_work);
	}

	if (IS_EVENT(msg, cloud, CLOUD_EVT_CONNECTION_TIMEOUT))
	{
		connect_aws();
	}
}

static void on_password_download_state_downloading(struct cloud_msg_data *msg)
{
	if (IS_EVENT(msg, download, DOWNLOAD_EVT_DOWNLOAD_FINISHED))
	{
		LOG_DBG("Password download complete.");
		handle_password_download_complete();
	}

	if (IS_EVENT(msg, download, DOWNLOAD_EVT_ERROR) || IS_EVENT(msg, download, DOWNLOAD_EVT_STORAGE_ERROR))
	{
		LOG_DBG("Error %d while downloading passwords.", msg->module.download.data.err);
		handle_password_download_failed();
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
 * @brief Initializes cloud related resources.
 * 
 * @return int 0 on success, negative errno code on failure.
 */
static int cloud_configure(void)
{
	sub_state_set(SUB_STATE_CLOUD_DISCONNECTED);
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
			switch (sub_state)
			{
			case SUB_STATE_CLOUD_CONNECTED:
				on_sub_state_cloud_connected(&msg);
				switch (pwd_download_state)
				{
				case DOWNLOAD_STATE_DOWNLOADING:
				{
					on_password_download_state_downloading(&msg);
				}
				}
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
EVENT_SUBSCRIBE(MODULE, download_module_event);
EVENT_SUBSCRIBE(MODULE, modem_module_event);