#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <caf/gpio_pins.h>

#define BTN_NODE DT_NODELABEL(button2)
#define BTN_PORT 0
#define BTN_PIN DT_GPIO_PIN(BTN_NODE, gpios)

static const struct gpio_pin col[] = {};

static const struct gpio_pin row[] = {
    {.port = BTN_PORT, .pin = BTN_PIN},
    {.port = BTN_PORT, .pin = DT_GPIO_PIN(DT_NODELABEL(button3), gpios)},
};