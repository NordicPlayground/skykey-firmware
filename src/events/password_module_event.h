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

#ifdef CONFIG_PASSWORD_MODULE
#define ENTRIES_MAX_LEN ((CONFIG_PASSWORD_ENTRY_MAX_NUM) * (CONFIG_PASSWORD_ENTRY_NAME_MAX_LEN + 1))
#else
#define ENTRIES_MAX_LEN 10
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
		enum password_module_event_type type;

		union
		{
			/* Module ID, used when acknowledging shutdown requests. */
			uint32_t id;
			int err;
			char entries[ENTRIES_MAX_LEN];
		} data;
	};

	EVENT_TYPE_DECLARE(password_module_event);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _PASSWORD_MODULE_EVENT_H_ */
