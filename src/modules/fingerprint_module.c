#include <zephyr.h>
#include <device.h>
#include <devicetree.h>

#include "../../drivers/fingerprint/api.h"

#define MODULE fingerprint_sensor
#include <event_manager.h>
#include <caf/events/click_event.h>
#include <caf/events/module_state_event.h>

static bool event_handler(const struct event_header *eh)
{
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
        const struct click_event *event = cast_click_event(eh);
        const struct device *dev = device_get_binding(DT_LABEL(DT_NODELABEL(fingerprint_sensor)));
        if (event->click == CLICK_DOUBLE)
        {
            static uint16_t id = 1;
            printk("Enroll started\n");
            if (enroll_finger(dev, id, K_MSEC(500), K_FOREVER, K_MSEC(2000))){
                printk("Enrolling failed.\n");
            } else {
                printk("Enrolling successfull\n");
                id++;
            }
        }
        if (event->click == CLICK_SHORT){
            printk("Identification started");
            int id = verify_finger(dev, K_MSEC(500), K_FOREVER);
            if (id <= 0){
                printk("Verification failed\n");
            } else {
                printk("Finger with id = %d verified\n", id);
            }
        }
        return false;
    }

    /* If event is unhandled, unsubscribe. */
    __ASSERT_NO_MSG(false);
    return false;
}

EVENT_LISTENER(MODULE, event_handler);
EVENT_SUBSCRIBE(MODULE, module_state_event);
EVENT_SUBSCRIBE(MODULE, click_event);