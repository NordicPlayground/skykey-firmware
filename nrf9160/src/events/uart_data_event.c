/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <assert.h>

#include "uart_data_event.h"

static int log_uart_data_event(const struct event_header *eh, char *buf,
				  size_t buf_len)
{
	const struct uart_data_event *event = cast_uart_data_event(eh);

	return snprintf(
		buf,
		buf_len,
		"dev:%u buf:%p len:%d",
		event->dev_idx,
		event->buf,
		event->len);
}

EVENT_TYPE_DEFINE(uart_data_event,
		  CONFIG_UART_DATA_EVENT_LOG_ENABLE,
		  log_uart_data_event,
		  NULL);
