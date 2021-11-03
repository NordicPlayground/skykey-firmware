#include <stdio.h>

#include "uart_transmit_event.h"

static int log_uart_data_event(const struct event_header *eh, char *buf,
				  size_t buf_len)
{
	const struct uart_transmit_event *event = cast_uart_transmit_event(eh);

	return snprintf(
		buf,
		buf_len,
		"dev:%u buf:%p len:%d",
		event->dev_idx,
		event->dyndata.data,
		event->dyndata.size);
}

EVENT_TYPE_DEFINE(uart_transmit_event,
		  CONFIG_UART_DATA_EVENT_LOG_ENABLE,
		  log_uart_data_event,
		  NULL);
