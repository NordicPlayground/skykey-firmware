/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr.h>
#include "event_manager.h"
#include <caf/events/click_event.h>


#define MODULE test

#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE);

static bool event_handler(const struct event_header *eh)
{
	if (is_click_event(eh)) {
		const struct click_event *event = cast_click_event(eh);
        return false;
	}
    return false;
}

EVENT_LISTENER(MODULE, event_handler);
EVENT_SUBSCRIBE(MODULE, click_event);