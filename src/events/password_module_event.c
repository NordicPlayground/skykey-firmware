/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>

#include "password_module_event.h"

static char *get_evt_type_str(enum password_module_event_type type)
{
	// TODO: Implement once all event types have been defined.
	switch (type)
	{
	case PASSWORD_EVT_REQ_DOWNLOAD:
		return "PASSWORD_EVT_REQ_DOWNLOAD";
	case PASSWORD_EVT_ERROR:
		return "PASSWORD_EVT_ERROR";
	default:
		return "";
	}
}

static int log_event(const struct event_header *eh, char *buf,
					 size_t buf_len)
{
	const struct password_module_event *event = cast_password_module_event(eh);
	switch (event->type)
	{
	case PASSWORD_EVT_REQ_DOWNLOAD:
		return snprintf(buf, buf_len, "%s: URL: %s", get_evt_type_str(event->type), event->data.download_params.url);
	default:
		return snprintf(buf, buf_len, "%s", get_evt_type_str(event->type));
	}
}

#if defined(CONFIG_PROFILER)

static void profile_event(struct log_event_buf *buf,
						  const struct event_header *eh)
{
	const struct password_module_event *event = cast_password_module_event(eh);

#if defined(CONFIG_PROFILER_EVENT_TYPE_STRING)
	profiler_log_encode_string(buf, get_evt_type_str(event->type),
							   strlen(get_evt_type_str(event->type)));
#else
	profiler_log_encode_u32(buf, event->type);
#endif
}

EVENT_INFO_DEFINE(password_module_event,
#if defined(CONFIG_PROFILER_EVENT_TYPE_STRING)
				  ENCODE(PROFILER_ARG_STRING),
#else
				  ENCODE(PROFILER_ARG_U32),
#endif
				  ENCODE("type"),
				  profile_event);

#endif /* CONFIG_PROFILER */

EVENT_TYPE_DEFINE(password_module_event,
				  CONFIG_PASSWORD_EVENTS_LOG,
				  log_event,
#if defined(CONFIG_PROFILER)
				  &password_module_event_info);
#else
				  NULL);
#endif
