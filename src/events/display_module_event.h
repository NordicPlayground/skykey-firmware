/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _DISPLAY_MODULE_EVENT_H_
#define _DISPLAY_MODULE_EVENT_H_

/**
 * @brief Display module event
 * @defgroup display_module_event Display module event
 * @{
 */

#include "event_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Display event types submitted by display module. */
enum display_module_event_type {
	DISPLAY_EVT_PLATFORM_CHOSEN,
	DISPLAY_EVT_ERROR
};

struct display_module_data {
	char* platform;
};

/** @brief UI event. */
struct display_module_event {
	struct event_header header;
	enum display_module_event_type type;

	union {
		struct display_module_data display;
		/* Module ID, used when acknowledging shutdown requests. */
		uint32_t id;
		int err;
	} data;
};

EVENT_TYPE_DECLARE(display_module_event);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _DISPLAY_MODULE_EVENT_H_ */
