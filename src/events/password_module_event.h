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
        PASSWORD_EVT_ERROR,
        PASSWORD_EVT_READ_PLATFORMS,
        PASSWORD_EVT_READ_CHOSEN_PASSWORD,
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
		/* DON'T MOVE THIS! Not having anything between the data union and dyndata struct causes builds to fail.
		My current theory is that it breaks dyndata aligment and triggers an assert that ensures that
		the dyndata field is at the end of the struct.*/
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
