/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr.h>
#include "event_manager.h"
#include <caf/events/button_event.h>


#define MODULE test

#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE);

static bool event_handler(const struct event_header *eh)
{
	if (is_button_event(eh)) {
		const struct button_event *event = cast_button_event(eh);
        LOG_INF("Button event from key_id: %d", event->key_id);
        return false;
	}
    return false;
}

EVENT_LISTENER(MODULE, event_handler);
EVENT_SUBSCRIBE(MODULE, button_event);