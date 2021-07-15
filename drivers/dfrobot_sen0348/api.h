#include <zephyr.h>
#include <device.h>
#include <drivers/uart.h>
#include <drivers/gpio.h>

#ifndef INCLUDE_FINGERPRINT_DRIVER_API_H
#define INCLUDE_FINGERPRINT_DRIVER_API_H

typedef enum
{
    Success = 0x00,              //Command processed successfully
    ErrorFail = 0x01,            //Command processing failed
    ErrorVerify = 0x10,          //1:1 Templates comparison in specific ID failed
    ErrorIdentify = 0x11,        //1:N comparison has been made, no same templates here
    ErrorTmplEmpty = 0x12,       //No registered template in the designated ID
    ErrorTmplNotEmpty = 0x13,    //Template already exists in the specified ID
    ErrorAllTmplEmpty = 0x14,    //No registered Template
    ErrorEmptyIDNoexist = 0x15,  //No registerable Template ID
    ErrorBrokenIDNoexist = 0x16, //No damaged Template
    ErrorInvalidTmplData = 0x17, //The designated Template Data is invalid
    ErrorDuplicationID = 0x18,   //The fingerprint has been registered
    ErrorBadQuality = 0x19,      //Poor quality fingerprint image
    ErrorMergeFail = 0x1A,       //Template synthesis failed
    ErrorNotAuthorized = 0x1B,   //Communication password not authorized
    ErrorMemory = 0x1C,          //Error in exernal Flash burning
    ErrorInvalidTmplNo = 0x1D,   //The designated template ID is invalid
    ErrorInvalidParam = 0x22,    //Incorrect parameter has been used
    ErrorTimeOut = 0x23,         //Acquisition timeout
    ErrorGenCount = 0x25,        //Invalid number of fingerprint synthesis
    ErrorInvalidBufferID = 0x26, //Wrong Buffer ID value
    ErrorFPNotDetected = 0x28,   //No fingerprint input into fingerprint reader
    ErrorFPCancel = 0x41,        //Command cancelled
    ErrorRecvLength = 0x42,      //Wrong length of recieved data
    ErrorRecvCks = 0x43,         //Wrong check code
    ErrorGatherOut = 0x45,       //Exceed upper limit of acquisition times
    ErrorRecvTimeout = 0x46,     //Communication timeout
    ERROR_ENUM_SPACER = 0xFFFF,
} fp_return_code;

typedef enum
{
    FP_IMG_FULL = 0,
    FP_IMG_QUARTER = 1,
} fp_img_format;

typedef enum
{
    FP_LED_COLOR_NONE = 0b000,
    FP_LED_COLOR_GREEN = 0b001,
    FP_LED_COLOR_RED = 0b010,
    FP_LED_COLOR_BLUE = 0b100,
    FP_LED_COLOR_YELLOW = FP_LED_COLOR_GREEN | FP_LED_COLOR_RED,
    FP_LED_COLOR_PURPLE = FP_LED_COLOR_BLUE | FP_LED_COLOR_RED,
    FP_LED_COLOR_CYAN = FP_LED_COLOR_BLUE | FP_LED_COLOR_GREEN,
    FP_LED_COLOR_WHITE = FP_LED_COLOR_BLUE | FP_LED_COLOR_GREEN | FP_LED_COLOR_RED,
} fp_led_color;

typedef enum
{
    FP_LED_BREATHE = 1,
    FP_LED_FLASH_FAST = 2,
    FP_LED_FLASH_SLOW = 7,
    FP_LED_ON = 3,
    FP_LED_OFF = 4,
    FP_LED_FADE_IN = 5,
    FP_LED_FADE_OUT = 6,
} fp_led_behaviour;

typedef int (*enroll_finger_t)(const struct device *dev, k_timeout_t op_timeout, k_timeout_t finger_timeout, k_timeout_t inter_finger_sleep);
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
 * @return 0 on success, -EBUSY if device is not available.
 */
inline int enroll_finger(const struct device *dev, k_timeout_t op_timeout, k_timeout_t finger_timeout, k_timeout_t inter_finger_sleep)
{
    struct fingerprint_api *api = (struct fingerprint_api *)dev->api;
    return api->enroll_finger(dev, op_timeout, finger_timeout, inter_finger_sleep);
}

/**
 * @brief Verifies finger on sensor against module database.
 * 
 * @param dev Fingerprint module device
 * @return 0 on successful verification.
 */
inline int verify_finger(const struct device *dev, k_timeout_t op_timeout, k_timeout_t finger_timeout)
{
    struct fingerprint_api *api = (struct fingerprint_api *)dev->api;
    return api->verify_finger(dev, op_timeout, finger_timeout);
}

struct fingerprint_module_data
{
};

#endif /*INCLUDE_FINGERPRINT_API_H*/