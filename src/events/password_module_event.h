/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _PASSWORD_MODULE_EVENT_H_
#define _PASSWORD_MODULE_EVENT_H_

/**
 * @brief Password module event
 * @defgroup password_module_event Password module event
 * @{
 */

#include "event_manager.h"

#ifdef __cplusplus
extern "C"
{
#endif

	/** @brief Password event types submitted by Password module. */
	enum password_module_event_type
	{
		PASSWORD_EVT_REQ_DOWNLOAD,
		PASSWORD_EVT_ERROR,
		PASSWORD_EVT_DOWNLOAD_STARTED,
		PASSWORD_EVT_DOWNLOAD_FINISHED,
		PASSWORD_EVT_DOWNLOAD_ERROR,
		PASSWORD_EVT_FLASH_ERROR,
	};

	/** @brief Password event. */
	struct password_module_event
	{
		struct event_header header;

		union
		{
			/* Module ID, used when acknowledging shutdown requests. */
			uint32_t id;
			int err;
		} data;

		enum password_module_event_type type;
		struct event_dyndata dyndata;
	};

	EVENT_TYPE_DYNDATA_DECLARE(password_module_event);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _PASSWORD_MODULE_EVENT_H_ */
