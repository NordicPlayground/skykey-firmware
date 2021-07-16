#pragma once
#include "api.h"

#define MSG_HDR_FIELD fp_msg_header hdr;
#define CHECKSUM_FIELD uint16_t checksum;

/** Opcodes for sensor commands.
NB!: All opcode aliases should be interpreted from the point of view of the sensor.
"Upload" means "upload from sensor to computer", "download" means "download from computer
to sensor" etc.*/
typedef enum
{
    TEST_CONNECTION = 0X0001,      //Test connection
    SET_PARAM = 0X0002,            //Set parameter
    GET_PARAM = 0X0003,            //Read parameter
    DEVICE_INFO = 0X0004,          //Read device information
    SET_MODULE_SN = 0X0008,        //Set module serial number
    GET_MODULE_SN = 0X0009,        //Read module serial number
    ENTER_STANDBY_STATE = 0X000C,  //Enter sleep mode
    GET_IMAGE = 0X0020,            //Capture fingerprint image
    FINGER_DETECT = 0X0021,        //Detect fingerprint
    UP_IMAGE = 0X0022,             //Upload fingerprint image to host
    DOWN_IMAGE = 0X0023,           //Download fingerprint image to module
    SLED_CTRL = 0X0024,            //Control collector backlight
    STORE_CHAR = 0X0040,           //Save fingerprint template data into fingerprint library
    LOAD_CHAR = 0X0041,            //Read fingerprint in module and save it in RAMBUFFER temporarily
    UP_CHAR = 0X0042,              //Upload the fingerprint template saved in RAMBUFFER to host
    DOWN_CHAR = 0X0043,            //Download fingerprint template to module designated RAMBUFFER
    DEL_CHAR = 0X0044,             //Delete fingerprint in specific ID range
    GET_EMPTY_ID = 0X0045,         //Get the first registerable ID in specific ID range
    GET_STATUS = 0X0046,           //Check if the designated ID has been registered
    GET_BROKEN_ID = 0X0047,        //Check whether there is damaged data in fingerprint library of specific range
    GET_ENROLL_COUNT = 0X0048,     //Get the number of registered fingerprints in specific ID range
    GET_ENROLLED_ID_LIST = 0X0049, //Get registered ID list
    GENERATE_CHAR = 0X0060,        //Generate template from the fingerprint images saved in IMAGEBUFFER temporarily
    MERGE = 0X0061,                //Synthesize fingerprint template data
    MATCH = 0X0062,                //Compare templates in 2 designated RAMBUFFER
    SEARCH = 0X0063,               //1:N Recognition in specific ID range
    VERIFY = 0X0064,               //Compare specific RAMBUFFER template with specific ID template in fingerprint library
    UNUSED = 0xffff,
} command_code;

typedef enum
{
    FP_PACKET_CMD = 0xaa55,
    FP_PACKET_RSP = 0x55aa,
    FP_PACKET_CMD_DATA = 0xa55a,
    FP_PACKET_RSP_DATA = 0x5aa5
} packet_type;

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
struct fp_msg_header
{
    packet_type type;
    uint8_t src_id;
    uint8_t dest_id;
    command_code opcode;
    uint16_t len;
} __packed;
typedef struct fp_msg_header fp_msg_header;

struct rsp_packet
{
    MSG_HDR_FIELD;
    fp_return_code status;
    uint8_t data[14];
    CHECKSUM_FIELD;
} __packed;
typedef const struct rsp_packet rsp_packet;

struct rsp_data_packet
{
    MSG_HDR_FIELD;
    fp_return_code status;
} __packed;

typedef const struct rsp_data_packet rsp_data_packet;
struct command_packet
{
    MSG_HDR_FIELD;
    uint8_t data[16];
    CHECKSUM_FIELD;
} __packed;
typedef struct command_packet cmd_packet;

struct command_data_packet
{
    MSG_HDR_FIELD;
} __packed;
typedef struct command_data_packet cmd_data_packet;

typedef struct
{
    unsigned int remaining_data_packets;
    size_t packet_buffer_index;
    uint8_t packet_buffer[CONFIG_SEN0348_MAX_PACKET_SIZE];
} data_accumulator;

struct finger_sync_struct
{
    struct k_mutex *mut;
    struct k_condvar *cond;
};

struct sen0348_conf
{
    struct k_mem_slab *tx_slab;
    struct k_mem_slab *rx_slab;
    const struct device *uart_dev;
    struct gpio_dt_spec irq_pin;
    struct gpio_dt_spec power_pin;
    uint8_t controller_id;
    uint8_t sensor_id;
    struct k_sem *transaction_sem;
    struct k_sem *cmd_sem;
    struct finger_sync_struct finger_sync;
    struct gpio_callback finger_gpio_callback;
    int status;
    int remaining_commands;
    data_accumulator accumulator;
};