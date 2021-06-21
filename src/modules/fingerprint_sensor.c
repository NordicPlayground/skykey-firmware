#include <zephyr.h>
#include "fingerprint_sensor.h"

#define MODULE fingerprint_sensor
#include <event_manager.h>
#include <caf/events/click_event.h>
#include <caf/events/module_state_event.h>

#include "events/uart_data_event.h"
#include "events/uart_transmit_event.h"

#define UART_ID CONFIG_FINGERPRINT_UART_ID

static collect_image_command tx_collect;
static download_image_command tx_dl;

static void send_data(void *data, size_t size, uint8_t dev_id)
{
    struct uart_transmit_event *event = new_uart_transmit_event(size);
    event->dev_idx = dev_id;
    memcpy(event->dyndata.data, data, event->dyndata.size);
    EVENT_SUBMIT(event);
}

static void handle_data(void *uart_buf, size_t len)
{
    collect_image_response response;
    fix_collect_image_response(&response, uart_buf);
    printk("Recieved %d bytes!\n", len);
    return;
}

static bool event_handler(const struct event_header *eh)
{
    int err;

    if (is_uart_data_event(eh))
    {
        const struct uart_data_event *event = cast_uart_data_event(eh);
        handle_data(event->buf, event->len);
        return false;
    }

    if (is_module_state_event(eh))
    {
        const struct module_state_event *event = cast_module_state_event(eh);

        if (check_state(event, MODULE_ID(main), MODULE_STATE_READY))
        {
            module_set_state(MODULE_STATE_READY);
        }

        return false;
    }

    if (is_click_event(eh))
    {
        struct click_event* event = cast_click_event(eh);
        if (event->click == CLICK_SHORT){
            init_collect_image_command(&tx_collect);
            send_data(&tx_collect, sizeof(tx_collect), UART_ID);
            return false;
        } else {
            init_download_image_command(&tx_dl);
            send_data(&tx_dl, sizeof(tx_dl), UART_ID);
            return false;
        }
    }

    /* If event is unhandled, unsubscribe. */
    __ASSERT_NO_MSG(false);
    return false;
}

EVENT_LISTENER(MODULE, event_handler);
EVENT_SUBSCRIBE(MODULE, module_state_event);
EVENT_SUBSCRIBE(MODULE, click_event);
EVENT_SUBSCRIBE(MODULE, uart_data_event);