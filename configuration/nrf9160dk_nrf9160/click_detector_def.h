#include <caf/key_id.h>
#include <caf/click_detector.h>

static const struct click_detector_config click_detector_config[] = {
    {
        .key_id = 0,
        .consume_button_event = false,
    },
    {
        .key_id = 1,
        .consume_button_event = false,
    },
};