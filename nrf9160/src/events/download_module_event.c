/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>

#include "download_module_event.h"

static char *get_evt_type_str(enum download_module_event_type type)
{
	// TODO: Implement once all event types have been defined.
	switch (type)
	{
	case DOWNLOAD_EVT_ERROR:
		return "DOWNLOAD_EVT_ERROR";
	case DOWNLOAD_EVT_DOWNLOAD_STARTED:
		return "DOWNLOAD_EVT_DOWNLOAD_STARTED";
	case DOWNLOAD_EVT_DOWNLOAD_FINISHED:
		return "DOWNLOAD_EVT_DOWNLOAD_FINISHED";
	case DOWNLOAD_EVT_STORAGE_ERROR:
		return "DOWNLOAD_EVT_STORAGE_ERROR";
	default:
		return "";
	}
}

static int log_event(const struct event_header *eh, char *buf,
					 size_t buf_len)
{
	const struct download_module_event *event = cast_download_module_event(eh);
	return snprintf(buf, buf_len, "%s", get_evt_type_str(event->type));
}

#if defined(CONFIG_PROFILER)

static void profile_event(struct log_event_buf *buf,
						  const struct event_header *eh)
{
	const struct download_module_event *event = cast_download_module_event(eh);

#if defined(CONFIG_PROFILER_EVENT_TYPE_STRING)
	profiler_log_encode_string(buf, get_evt_type_str(event->type),
							   strlen(get_evt_type_str(event->type)));
#else
	profiler_log_encode_u32(buf, event->type);
#endif
}

EVENT_INFO_DEFINE(download_module_event,
#if defined(CONFIG_PROFILER_EVENT_TYPE_STRING)
				  ENCODE(PROFILER_ARG_STRING),
#else
				  ENCODE(PROFILER_ARG_U32),
#endif
				  ENCODE("type"),
				  profile_event);

#endif /* CONFIG_PROFILER */

EVENT_TYPE_DEFINE(download_module_event,
				  CONFIG_DOWNLOAD_EVENTS_LOG,
				  log_event,
#if defined(CONFIG_PROFILER)
				  &download_module_event_info);
#else
				  NULL);
#endif
