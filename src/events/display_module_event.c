/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>

#include "display_module_event.h"

static char *get_evt_type_str(enum display_module_event_type type)
{
	switch (type) {
	case DISPLAY_EVT_PLATFORM_CHOSEN:
		return "DISPLAY_EVT_PLATFORM_CHOSEN";
	case DISPLAY_EVT_REQUEST_PLATFORMS:
		return "DISPLAY_EVT_REQUEST_PLATFORMS";
	case DISPLAY_EVT_ERROR : return "DISPLAY_EVT_ERROR";
	default:
		return "Unknown event";
	}
}

static int log_event(const struct event_header *eh, char *buf,
		     size_t buf_len)
{
	const struct display_module_event *event = cast_display_module_event(eh);

	if (event->type == DISPLAY_EVT_ERROR) {
		return snprintf(buf, buf_len, "%s - Error code %d",
				get_evt_type_str(event->type), event->data.err);
	}

	return snprintf(buf, buf_len, "%s", get_evt_type_str(event->type));
}

#if defined(CONFIG_PROFILER)

static void profile_event(struct log_event_buf *buf,
			 const struct event_header *eh)
{
	const struct display_module_event *event = cast_display_module_event(eh);

#if defined(CONFIG_PROFILER_EVENT_TYPE_STRING)
	profiler_log_encode_string(buf, get_evt_type_str(event->type),
		strlen(get_evt_type_str(event->type)));
#else
	profiler_log_encode_u32(buf, event->type);
#endif
}

EVENT_INFO_DEFINE(display_module_event,
#if defined(CONFIG_PROFILER_EVENT_TYPE_STRING)
		  ENCODE(PROFILER_ARG_STRING),
#else
		  ENCODE(PROFILER_ARG_U32),
#endif
		  ENCODE("type"),
		  profile_event);

#endif /* CONFIG_PROFILER */

EVENT_TYPE_DEFINE(display_module_event,
		  CONFIG_DISPLAY_EVENTS_LOG,
		  log_event,
#if defined(CONFIG_PROFILER)
		  &display_module_event_info);
#else
		  NULL);
#endif
