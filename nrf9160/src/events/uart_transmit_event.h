#pragma once

/**
 * @brief UART Transmit Event
 * @defgroup uart_transmit_event UART Transmit Event
 * @{
 */

#include "event_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum alloc_type {
	GLOBAL_VAR,
	SLAB,
	HEAP
} alloc_t;

/** Uart transmission request */
struct uart_transmit_event {
	struct event_header header;

	uint8_t dev_idx;
	struct event_dyndata dyndata;
};

EVENT_TYPE_DYNDATA_DECLARE(uart_transmit_event);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

