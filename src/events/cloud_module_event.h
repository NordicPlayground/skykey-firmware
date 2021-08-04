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
		CLOUD_EVT_DOWNLOAD_STARTED,
		CLOUD_EVT_DOWNLOAD_FINISHED,
		CLOUD_EVT_DOWNLOAD_ERROR,
	};

	/**
	 * @brief Contains parameters for file downloads.
	 * URL is given in dyndata as a null terminated string.
	 */
	struct cloud_module_download_params
	{
		int64_t version;
	};

	struct cloud_module_event_data
	{
		char *buf;
		size_t len;
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

		union
		{
			struct cloud_module_download_params download_params;
			/* Module ID, used when acknowledging shutdown requests. */
			uint32_t id;
			int err;
			char url[50];
		} data;

		/* DON'T MOVE THIS! Not having anything between the data union and dyndata struct causes builds to fail.
		My current theory is that it breaks dyndata aligment and triggers an assert that ensures that
		the dyndata field is at the end of the struct.*/
		enum cloud_module_event_type type;

		// struct event_dyndata dyndata;
	};

	EVENT_TYPE_DECLARE(cloud_module_event);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _CLOUD_MODULE_EVENT_H_ */
