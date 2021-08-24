/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _DOWNLOAD_MODULE_EVENT_H_
#define _DOWNLOAD_MODULE_EVENT_H_

/**
 * @brief Download module event
 * @defgroup download_module_event Download module event
 * @{
 */

#include "event_manager.h"

#ifdef __cplusplus
extern "C"
{
#endif

	/** @brief Download event types submitted by Download module. */
	enum download_module_event_type
	{
		DOWNLOAD_EVT_REQ_DOWNLOAD,
		DOWNLOAD_EVT_DOWNLOAD_STARTED,
		DOWNLOAD_EVT_DOWNLOAD_FINISHED,
		DOWNLOAD_EVT_ERROR,
		DOWNLOAD_EVT_STORAGE_ERROR,
		DOWNLOAD_EVT_SHUTDOWN_READY,
	};

	/** @brief Download event. */
	struct download_module_event
	{
		struct event_header header;
		enum download_module_event_type type;
		union
		{
			/* Module ID, used when acknowledging shutdown requests. */
			uint32_t id;
			int err;
		} data;
	};

	EVENT_TYPE_DECLARE(download_module_event);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _DOWNLOAD_MODULE_EVENT_H_ */
