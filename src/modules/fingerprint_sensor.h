#pragma once
#include <zephyr.h>
#include <sys/byteorder.h>

//// General

// Fingerprint sensor transfers data in big endian format.
// These macros are used to perform any necessary corrections.
#define endian16(int16) sys_cpu_to_be16(int16)
#define endian32(int32) sys_cpu_to_be32(int32)

#define SENSOR_ADDR endian32(CONFIG_FINGERPRINT_MODULE_ADDRESS)
#define HEADER_FIELD_VAL endian16(0xef01)

#define FP_DATA_HEADER_FIELDS \
    uint16_t hdr;             \
    uint32_t addr;            \
    uint8_t pid;              \
    uint16_t len;

// fp_checksum_field get_command_checksum(uint16_t *area_start, size_t size)
// {
//     uint16_t sum = 0;
//     for (size_t i = 0; i < size; i++)
//     {
//         sum += area_start[i];
//     }
//     return sum;
// }

// #define get_checksum(message) get_command_checksum((uint16_t *)&message->pid, (size_t)&message->checksum - (size_t)&message->pid)

//// Command specific
enum command_code
{
    COLLECT_IMAGE = 0x01,
    IMG_TO_CHAR_FILE = 0x02,
    MATCH = 0x03,
    SEARCH = 0x04,
    GENERATE_TEMPLATE = 0x05,
    /*Stores template from sensor buffer to sensor flash*/
    STORE_TEMPLATE_FROM_BUFFER = 0x06,
    /*Loads template from sensor flash to sensor buffer*/
    LOAD_TEMPLATE = 0x07,
    /*Upload template to sensor*/
    UPLOAD_TEMPLATE = 0x08,
    /*Download template from sensor*/
    DOWNLOAD_TEMPLATE = 0x09,
    UPLOAD_IMAGE = 0x0a,
    DOWNLOAD_IMAGE = 0x0b,
    DELETE_TEMPLATE = 0x0c,
    EMPTY_LIBRARY = 0x0d,
    SET_SYSTEM_PARAMETER = 0x0e,
    READ_SYSTEM_PARAMETER = 0x0f,
    GET_RANDOM_CODE = 0x14,
    SET_ADDRESS = 0x15,
    HANDSHAKE = 0x17,
    WRITE_NOTEPAD = 0x18,
    READ_NOTEPAD = 0x19,
    READ_VALID_TEMPLATE_NUMBER = 0x1D,
};

#define FP_COMMAND_FIELDS \
    FP_DATA_HEADER_FIELDS \
    enum command_code opcode;

#define CHECKSUM_FIELD uint16_t checksum;

typedef struct
{
    FP_COMMAND_FIELDS;
    CHECKSUM_FIELD;
} __packed basic_command;

basic_command *init_basic_command(basic_command *cmd_ptr, enum command_code opcode)
{
    cmd_ptr->hdr = HEADER_FIELD_VAL;
    cmd_ptr->addr = SENSOR_ADDR;
    cmd_ptr->pid = 1;
    cmd_ptr->len = endian16(3);
    cmd_ptr->opcode = opcode;
    uint16_t checksum = cmd_ptr->pid + endian16(cmd_ptr->len) + cmd_ptr->opcode;
    cmd_ptr->checksum = endian16(checksum);
    return cmd_ptr;
}

/*Defines a datatype and an init function for a command with no special fields*/
#define DEFINE_BASIC_COMMAND(name, opcode)                         \
    typedef basic_command name##_command;                          \
    name##_command *init_##name##_command(name##_command *cmd_ptr) \
    {                                                              \
        return init_basic_command(cmd_ptr, opcode);                \
    }

typedef struct
{
    FP_COMMAND_FIELDS;
    uint8_t control_code;
    CHECKSUM_FIELD;
} __packed handshake_command;

handshake_command *init_handshake_command(handshake_command *cmd_ptr)
{
    cmd_ptr->hdr = HEADER_FIELD_VAL;
    cmd_ptr->addr = SENSOR_ADDR;
    cmd_ptr->pid = 1;
    cmd_ptr->len = endian16(4);
    cmd_ptr->opcode = HANDSHAKE;
    cmd_ptr->control_code = 0;
    cmd_ptr->checksum = endian16(0x001c);
    return cmd_ptr;
}

typedef struct
{
    FP_COMMAND_FIELDS;
    uint32_t new_address;
    CHECKSUM_FIELD;
} __packed set_address_command;

set_address_command *init_set_address_command(set_address_command *cmd_ptr, uint32_t address)
{
    cmd_ptr->hdr = HEADER_FIELD_VAL;
    cmd_ptr->addr = SENSOR_ADDR;
    cmd_ptr->pid = 1;
    uint16_t length = sizeof(cmd_ptr->opcode) + sizeof(cmd_ptr->new_address) + sizeof(cmd_ptr->checksum);
    cmd_ptr->len = endian16(length);
    cmd_ptr->opcode = HANDSHAKE;
    cmd_ptr->new_address = endian32(address);
    uint16_t checksum = address + cmd_ptr->opcode + cmd_ptr->pid + endian16(cmd_ptr->len);
    cmd_ptr->checksum = endian16(checksum);
    return cmd_ptr;
}

typedef enum
{
    BAUD_RATE = 4,
    SECURITY_LEVEL = 5,
    DATA_PKG_LEN = 6,
} fp_sys_param;

typedef struct
{
    FP_COMMAND_FIELDS;
    uint32_t new_address;
    CHECKSUM_FIELD;
} __packed set_sys_param_command;

set_sys_param_command *init_set_sys_param_command(set_sys_param_command *cmd_ptr, uint32_t address)
{
    cmd_ptr->hdr = HEADER_FIELD_VAL;
    cmd_ptr->addr = SENSOR_ADDR;
    cmd_ptr->pid = 1;
    uint16_t length = sizeof(cmd_ptr->opcode) + sizeof(cmd_ptr->new_address) + sizeof(cmd_ptr->checksum);
    cmd_ptr->len = endian16(length);
    cmd_ptr->opcode = HANDSHAKE;
    cmd_ptr->new_address = endian32(address);
    uint16_t checksum = address + cmd_ptr->opcode + cmd_ptr->pid + endian16(cmd_ptr->len);
    cmd_ptr->checksum = endian16(checksum);
    return cmd_ptr;
}

DEFINE_BASIC_COMMAND(collect_image, COLLECT_IMAGE);
DEFINE_BASIC_COMMAND(download_image, DOWNLOAD_IMAGE);

//// Response specific

enum confirmation_code
{
    COMPLETE = 0x0,
    /*Error when recieving data package*/
    ERROR_ON_RECIEVE = 0x01,
    NO_FINGER_ON_SENSOR = 0x02,
    FAILED_TO_ENROLL = 0x03,
    /*Failed to generate template due to unclear/disroderly image*/
    UNCLEAR_IMAGE = 0x06,
    /*Failed to generate template due to lack of detail in image (?)*/
    SPARSE_IMAGE = 0x07,
    /*Templates in buffers don't match*/
    BUFFERS_DONT_MATCH = 0x08,
    /*Library doesn't contain a match*/
    NO_MATCH_FOUND = 0x09,

};

#define FP_RESPONSE_FIELDS \
    FP_DATA_HEADER_FIELDS; \
    enum confirmation_code status;

struct basic_response
{
    FP_RESPONSE_FIELDS;
    CHECKSUM_FIELD;
} __packed;

/*Extracts basic response from src, fixes endianness, and writes result to dest*/
struct basic_response *fix_basic_response(struct basic_response *dest, struct basic_response *src)
{
    dest->hdr = endian16(src->hdr);
    dest->addr = endian32(src->addr);
    dest->pid = src->pid;
    dest->len = endian16(src->len);
    dest->status = src->status;
    dest->checksum = endian16(src->checksum);
    return dest;
}

/*Defines a datatype and a recovery function for a response with no special fields*/
#define DEFINE_BASIC_RESPONSE(name)                                                     \
    typedef struct basic_response name##_response;                                      \
    name##_response *fix_##name##_response(name##_response *dest, name##_response *src) \
    {                                                                                   \
        return fix_basic_response(dest, src);                                           \
    }

DEFINE_BASIC_RESPONSE(handshake);
DEFINE_BASIC_RESPONSE(collect_image);
DEFINE_BASIC_RESPONSE(upload_image);
DEFINE_BASIC_RESPONSE(download_image);