/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _CLOUD_MODULE_EVENT_H_
#define _CLOUD_MODULE_EVENT_H_

/**
 * @brief Cloud module event
 * @defgroup cloud_module_event Cloud module event
 * @{
 */

#include "event_manager.h"

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef CONFIG_CLOUD_MODULE
#define URL_MAX_LEN CONFIG_CLOUD_DOWNLOAD_URL_MAX_LEN
#else
#define URL_MAX_LEN 10
#endif

/** @brief Cloud event types submitted by Cloud module. */
enum cloud_module_event_type
{
	CLOUD_EVT_CONNECTED,
	CLOUD_EVT_DISCONNECTED,
	CLOUD_EVT_CONNECTING,
	CLOUD_EVT_CONNECTION_TIMEOUT,
	CLOUD_EVT_CONFIG_RECEIVED,
	CLOUD_EVT_CONFIG_EMPTY,
	CLOUD_EVT_FOTA_DONE,
	CLOUD_EVT_DATA_ACK,
	CLOUD_EVT_SHUTDOWN_READY,
	CLOUD_EVT_ERROR,
	CLOUD_EVT_LTE_CONNECTED,
	CLOUD_EVT_LTE_DISCONNECTED,
	CLOUD_EVT_DATABASE_UPDATE_AVAILABLE,
	CLOUD_EVT_NEW_LOCK_TIMEOUT,
};

struct cloud_module_data_ack
{
	void *ptr;
	size_t len;
	/* Flag to signify if the data was sent or not. */
	bool sent;
};

/** @brief Cloud event. */
struct cloud_module_event
{
	struct event_header header;
	enum cloud_module_event_type type;
	union
	{
		/* Module ID, used when acknowledging shutdown requests. */
		uint32_t id;
		int err;
		uint32_t timeout;
		char url[URL_MAX_LEN];
	} data;
};

EVENT_TYPE_DECLARE(cloud_module_event);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _CLOUD_MODULE_EVENT_H_ */
