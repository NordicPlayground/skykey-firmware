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
#include <net/download_client.h>

#include "events/cloud_module_event.h"
#include "events/password_module_event.h"

#define MODULE cloud_module

#include "modules_common.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_CLOUD_MODULE_LOG_LEVEL);

struct cloud_msg_data
{
	union
	{
		struct cloud_module_event cloud;
		struct password_module_event password;
	} module;
	void *data;
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

/* Download client state. */
static enum download_state_type { DOWNLOAD_STATE_READY,
								  DOWNLOAD_STATE_BUSY,
} download_state;

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

static char *dl_state2str(enum download_state_type new_state)
{
	switch (new_state)
	{
	case DOWNLOAD_STATE_READY:
		return "DOWNLOAD_STATE_READY";
	case DOWNLOAD_STATE_BUSY:
		return "DOWNLOAD_STATE_BUSY";
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

static void dl_state_set(enum download_state_type new_state)
{
	if (new_state == download_state)
	{
		LOG_DBG("Download state: %s", dl_state2str(download_state));
		return;
	}

	LOG_DBG("Download state transition: %s --> %s",
			dl_state2str(download_state),
			dl_state2str(new_state));
	download_state = new_state;
}

//========================================================================================
/*                                                                                      *
 *                               AWS related functionality                              *
 *                                                                                      */
//========================================================================================

// TODO: Find a better way to check for topics + remove hardcoded ID.
#define AWS_IOT_TOPIC_SHADOW_UPDATE_DELTA "$aws/things/352656109498066/shadow/update/delta"
#define AWS_IOT_TOPIC_SHADOW_GET_ACCEPTED "$aws/things/352656109498066/shadow/get/accepted"

/**
 * @brief Work item used to check if a cloud connection is established.
 */
static struct k_work_delayable connect_check_work;

static struct download_client dl_client;
static struct download_client_cfg dl_client_cfg = {
	.sec_tag = -1,
	.apn = NULL,
	.pdn_id = 0,
	.frag_size_override = 0,
	.set_tls_hostname = false,
};

/**
 * @brief Function for handling file fragments. 
 * Is provided by password module when requesting a download.
 */
static password_download_cb_t download_callback = NULL;

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

	// Dummy value for testing/POC.
	cJSON *val_obj = cJSON_GetObjectItemCaseSensitive(delta, "value");
	if (val_obj != NULL)
	{
		NOT_NULL(cJSON_AddNumberToObject(reported, "value", val_obj->valueint), root, -ENOMEM);
	}

	// TODO: Consider using AWS IoT Jobs instead of device shadow for updating database.
	//This is a temporary solution for testing the downloading mechanism.
	cJSON *delta_obj = cJSON_GetObjectItemCaseSensitive(delta, "desired");
	cJSON *pwd_obj = cJSON_GetObjectItemCaseSensitive(delta_obj, "pwd");
	if (pwd_obj != NULL)
	{
		cJSON *pwd_url_obj = cJSON_GetObjectItemCaseSensitive(pwd_obj, "url");
		cJSON *pwd_ver_obj = cJSON_GetObjectItemCaseSensitive(pwd_obj, "v");
		if (pwd_url_obj != NULL && pwd_ver_obj != NULL)
		{
			char *url = pwd_url_obj->valuestring;
			int64_t version = pwd_ver_obj->valueint;
			struct cloud_module_event *event = new_cloud_module_event(); // +1 for null terminator.
			event->type = CLOUD_EVT_DATABASE_UPDATE_AVAILABLE;
			// event->data.download_params.version = version;
			strcpy(event->data.url, url);
			LOG_WRN("Sending event");
			EVENT_SUBMIT(event);
		}
		cJSON_AddItemReferenceToObject(reported, "pwd", pwd_obj); // HACK: Echo password settings to remove it from delta. Again, this is not a good solution.
	}

	char *message = cJSON_Print(root);
	NOT_NULL(message, root, -ENOMEM);

	struct aws_iot_data tx_data = {
		.qos = MQTT_QOS_0_AT_MOST_ONCE,
		.topic.type = AWS_IOT_SHADOW_TOPIC_UPDATE,
		.ptr = message,
		.len = strlen(message)};

	LOG_DBG("Updating shadow");
	// int err = aws_iot_send(&tx_data);

	cJSON_free(message);
	cJSON_Delete(root);
	int err = 0;
	return err;
}
#undef NOT_NULL

static void handle_cloud_data(const struct aws_iot_data *msg)
{
	if (!strcmp(msg->topic.str, AWS_IOT_TOPIC_SHADOW_GET_ACCEPTED))
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

// This function might be redundant, but I sense some looming atomicity issues just around the corner.
/**
 * @brief Set the download callback.
 * 
 * @param new_cb 
 */
static void set_download_callback(password_download_cb_t new_cb)
{
	download_callback = new_cb;
}

/**
 * @brief Notfies download callback about new data.
 * 
 * @param fragment Pointer to file fragment.
 * @param size Size of file fragment.
 * @param file_size Total size of file.
 * 
 * @return Negative error code if download should be canceled, 0 or positive otherwise.
 * If callback is NULL this will always return 0.
 */
static int notify_download_callback(const void * const fragment, size_t frag_size, size_t file_size)
{
	if (download_callback == NULL)
	{
		return 0;
	}
	return download_callback(fragment, frag_size, file_size);
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
		/* Update current time. */
		date_time_update_async(NULL);

		printk("Attempting to connect to AWS");
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

/* Message handler for DOWNLOAD_STATE_READY*/
static void on_dl_state_ready(struct cloud_msg_data *msg)
{
	if (IS_EVENT(msg, password, PASSWORD_EVT_REQ_DOWNLOAD))
	{
		//char *url = msg->data;
		set_download_callback(msg->module.password.data.download_params.callback);
		int err = download_client_connect(&dl_client, msg->module.password.data.download_params.url, &dl_client_cfg);
		if (err < 0)
		{
			SEND_ERROR(cloud, CLOUD_EVT_DOWNLOAD_ERROR, err);
		}
		else
		{
			err = download_client_start(&dl_client, msg->module.password.data.download_params.url, 0);
			if (err < 0)
			{
				SEND_ERROR(cloud, CLOUD_EVT_DOWNLOAD_ERROR, err);
			}
			else
			{
				SEND_EVENT(cloud, CLOUD_EVT_DOWNLOAD_STARTED);
				dl_state_set(DOWNLOAD_STATE_BUSY);
			}
		}
	}
}

/* Message handler for DOWNLOAD_STATE_BUSY*/
static void on_dl_state_busy(struct cloud_msg_data *msg)
{
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

	if (is_password_module_event(eh))
	{
		struct password_module_event *evt = cast_password_module_event(eh);
		msg.module.password = *evt;
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
	{

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
	}
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

static int download_client_callback(const struct download_client_evt *evt)
{
	switch (evt->id)
	{
	case DOWNLOAD_CLIENT_EVT_FRAGMENT:
	{
		LOG_DBG("Fragment recieved. Size: %d", evt->fragment.len);
		size_t file_size;
		download_client_file_size_get(&dl_client, &file_size);
		notify_download_callback(evt->fragment.buf, evt->fragment.len, file_size);
		break;
	}
	case DOWNLOAD_CLIENT_EVT_DONE:
	{
		SEND_EVENT(cloud, CLOUD_EVT_DOWNLOAD_FINISHED);
		LOG_DBG("Download complete");
		break;
	}
	case DOWNLOAD_CLIENT_EVT_ERROR:
	{
		SEND_ERROR(cloud, CLOUD_EVT_DOWNLOAD_ERROR, evt->error);
		LOG_WRN("An error occured while downloading: %d", evt->error);
		break;
	}
	}
	return 0;
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
	if (err)
	{
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
	download_client_init(&dl_client, download_client_callback);
	dl_state_set(DOWNLOAD_STATE_READY);
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

			switch (download_state)
			{
			case DOWNLOAD_STATE_READY:
				on_dl_state_ready(&msg);
				break;
			case DOWNLOAD_STATE_BUSY:
				on_dl_state_busy(&msg);
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
		k_free(msg.data); // Free dynamic data
	}
}

K_THREAD_DEFINE(cloud_module_thread, CONFIG_CLOUD_THREAD_STACK_SIZE,
				module_thread_fn, NULL, NULL, NULL,
				K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);

EVENT_LISTENER(MODULE, event_handler);
EVENT_SUBSCRIBE(MODULE, cloud_module_event);
EVENT_SUBSCRIBE(MODULE, password_module_event);