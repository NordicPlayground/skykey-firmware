#include <zephyr.h>
#include <device.h>
#include <drivers/uart.h>
#include <drivers/gpio.h>

#ifndef INCLUDE_FINGERPRINT_DRIVER_API_H
#define INCLUDE_FINGERPRINT_DRIVER_API_H


typedef int (*enroll_finger_t)(const struct device *dev, uint16_t id, k_timeout_t op_timeout, k_timeout_t finger_timeout, k_timeout_t inter_finger_sleep);
typedef int (*verify_finger_t)(const struct device *dev, k_timeout_t op_timeout, k_timeout_t finger_timeout);

struct fingerprint_api
{
    enroll_finger_t enroll_finger;
    verify_finger_t verify_finger;
};

/**
 * @brief Generates a template from finger on sensor and stores it in a buffer on the module.
 * Finger must be moved on the sensor when indicated.
 * 
 * @param dev Fingerprint sensor device
 * @param id ID to be assigned to enrolled finger
 * @param op_timeout Timeout for module operations
 * @param finger_timeout Max time spent waiting for finger after `inter_finger_sleep`
 * @param inter_finger_sleep Min time spent waiting for finger
 * @return 0 on success, -EBUSY if device is not available.
 */
static inline int enroll_finger(const struct device *dev, uint16_t id, k_timeout_t op_timeout, k_timeout_t finger_timeout, k_timeout_t inter_finger_sleep)
{
    struct fingerprint_api *api = (struct fingerprint_api *)dev->api;
    return api->enroll_finger(dev, id, op_timeout, finger_timeout, inter_finger_sleep);
}

/**
 * @brief Verifies finger on sensor against module database.
 * 
 * @param dev Fingerprint module device
 * @param op_timeout Timeout for module operations
 * @param finger_timeout Max time spent waiting for finger after `inter_finger_sleep`
 * @return Negative on failure, positive on succes. Positive values corespond to the ID if the identified finger. 
 */
static inline int verify_finger(const struct device *dev, k_timeout_t op_timeout, k_timeout_t finger_timeout)
{
    struct fingerprint_api *api = (struct fingerprint_api *)dev->api;
    return api->verify_finger(dev, op_timeout, finger_timeout);
}

struct fingerprint_module_data
{
};

#endif /*INCLUDE_FINGERPRINT_API_H*/