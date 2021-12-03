
#define SEN3048_SENSOR_INIT_PRIORITY 41
#include <kernel.h>
#include <string.h>
#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <sys/byteorder.h>
#include <errno.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(sen0348_driver, CONFIG_SEN0348_DRIVER_LOG_LEVEL);

#include "sen0348.h"
#define DT_DRV_COMPAT dfrobot_sen0348
#define UART_SLAB_ALIGNMENT 4
#define DATA_PACKET_CAPACITY 496

#define endian32(num) sys_cpu_to_le32(num)
#define endian16(num) sys_cpu_to_le16(num)

static uint16_t get_checksum(uint8_t *packet, size_t size)
{
    uint16_t checksum = 0;
    for (int i = 0; i < size; i++)
    {
        checksum += packet[i];
    }
    return checksum;
}

static void init_packet_hdr(fp_msg_header *packet, packet_type type, command_code opcode, uint8_t src_id, uint8_t dest_id, size_t data_len)
{
    packet->type = endian16(type);
    packet->src_id = src_id;
    packet->dest_id = dest_id;
    packet->opcode = endian16(opcode);
    packet->len = endian16(data_len);
}

static uint16_t init_cmd_packet(cmd_packet *packet, command_code opcode, uint8_t src_id, uint8_t dest_id, uint8_t data[16], uint16_t data_len)
{
    memset(packet, 0, sizeof(cmd_packet));
    init_packet_hdr(&packet->hdr, FP_PACKET_CMD, opcode, src_id, dest_id, data_len);
    if (data != NULL)
    {
        memcpy(packet->data, data, data_len);
    }
    uint16_t checksum = get_checksum((uint8_t *)packet, sizeof(cmd_packet));
    packet->checksum = endian16(checksum);
    return checksum;
}

/**
 * @brief Signals start of transaction with @p command_count commands.
 * 
 * @param dev Fingerprint module device
 * @param command_count Number of commands in transaction. Set to 0 or below to allow an arbitrary number of commands.
 * @param timeout Timeout
 * @return 0 on success, -EBUSY if timeout expires.
 */
static inline int start_transaction(const struct device *dev, int command_count, k_timeout_t timeout)
{
    struct sen0348_conf *conf = (struct sen0348_conf *)dev->config;
    conf->remaining_commands = command_count;
    int err = k_sem_take(conf->transaction_sem, timeout);
    return err ? -EBUSY : 0;
}

/**
 * @brief Completes the current transaction.
 * 
 * @param dev Fingerprint module device
 * @return 0 on success, -EAGAIN if transaction is not finished.
 */
static inline int complete_transaction(const struct device *dev)
{
    struct sen0348_conf *conf = (struct sen0348_conf *)dev->config;
    k_sem_give(conf->transaction_sem);
    return 0;
}

/**
 * @brief Signals start of command.
 * 
 * @param dev Fingerprint module device
 * @param timeout Timeout
 */
static inline int start_command(const struct device *dev, k_timeout_t timeout)
{
    struct sen0348_conf *conf = (struct sen0348_conf *)dev->config;
    int err = k_sem_take(conf->cmd_sem, timeout);
    return err ? -EBUSY : 0;
}

/**
 * @brief Completes current command.
 * 
 * @param dev Fingerprint module device
 * @return 0
 */
static inline int complete_command(const struct device *dev)
{
    struct sen0348_conf *conf = (struct sen0348_conf *)dev->config;
    k_sem_give(conf->cmd_sem);
    return 0;
}

/**
 * @brief Wait until a finger is detected on the sensor.
 * 
 * @param dev Fingerprint module device
 * @param timeout Timeout
 * @return 0 on success, -EBUSY when returning without waiting, -EAGAIN for timeout
 */
static int wait_for_finger(const struct device *dev, k_timeout_t timeout)
{
    struct sen0348_conf *conf = (struct sen0348_conf *)dev->config;
    return k_condvar_wait(conf->finger_sync.cond, conf->finger_sync.mut, timeout);
}

static void finger_int_handler(const struct device *port, struct gpio_callback *cb, gpio_port_pins_t pins)
{
    struct sen0348_conf *conf = CONTAINER_OF(cb, struct sen0348_conf, finger_gpio_callback);
    k_condvar_broadcast(conf->finger_sync.cond);
}

/**
 * @brief Clears packet device's packet accumulator
 * 
 * @param dev Fingerprint module device
 */
static void clear_accumulator(const struct device *dev)
{
    struct sen0348_conf *conf = (struct sen0348_conf *)dev->config;
    data_accumulator *acc = &conf->accumulator;
    memset(acc->packet_buffer, 0, sizeof(acc->packet_buffer));
    acc->packet_buffer_index = 0;
}

/**
 * @brief Accumulates data into sen0348 response packets. Also handles counting 
 * data packets to ensure that a complete response has been recieved.
 * 
 * @param fp_dev Fingerprint sensor device
 * @param data Pointer to data
 * @param len Length of data
 * @param offset Start of unhandled data
 * @return `true` if a complete response, including potential data packets, could be extracted, `false` otherwise
 */
static bool _accumulate_response(const struct device *fp_dev, void *data, size_t len, size_t offset)
{
    struct sen0348_conf *conf = (struct sen0348_conf *)fp_dev->config;
    data_accumulator *acc = &conf->accumulator;
    fp_msg_header *packet_hdr = (fp_msg_header *)conf->accumulator.packet_buffer;
    int data_index = offset;
    len += offset;
    bool response_ready = false;
    while (data_index < len)
    {
        bool packet_ready = false;
        if (conf->accumulator.packet_buffer_index < sizeof(fp_msg_header))
        {
            size_t data_handled = MIN(len - data_index, sizeof(fp_msg_header) - acc->packet_buffer_index);
            memcpy(&(acc->packet_buffer[acc->packet_buffer_index]), (uint8_t *)data + data_index, data_handled);
            data_index += data_handled;
            acc->packet_buffer_index += data_handled;
            continue;
        }
        else
        {
            switch (packet_hdr->type)
            {
            case FP_PACKET_RSP:
            {
                size_t data_handled = MIN(len - data_index, sizeof(rsp_packet) - acc->packet_buffer_index);
                memcpy(&(acc->packet_buffer[acc->packet_buffer_index]), (uint8_t *)data + data_index, data_handled);
                data_index += data_handled;
                acc->packet_buffer_index += data_handled;
                if (acc->packet_buffer_index == sizeof(rsp_packet))
                {
                    rsp_packet *packet = (rsp_packet *)packet_hdr;
                    packet_ready = true;
                    switch (packet->hdr.opcode)
                    {
                    case UP_CHAR:
                    {
                        LOG_WRN("NOT IMPLEMENTED!");
                        if (packet->status == Success)
                        {
                            //TODO: Figure out how many data packets will arrive
                            acc->remaining_data_packets = 1;
                        }
                        else
                        {
                            acc->remaining_data_packets = 0;
                            response_ready = true;
                        }
                        break;
                    }
                    case UP_IMAGE:
                    {
                        if (packet->status == Success)
                        {
                            uint16_t width = (uint16_t)packet->data[0];
                            uint16_t height = (uint16_t)packet->data[2];
                            acc->remaining_data_packets = width * height / DATA_PACKET_CAPACITY;
                            if (width * height % DATA_PACKET_CAPACITY != 0)
                            {
                                acc->remaining_data_packets++;
                            }
                            LOG_DBG("Image download started. Expecting %d data packets", acc->remaining_data_packets);
                        }
                        else
                        {
                            acc->remaining_data_packets = 0;
                            response_ready = true;
                        }
                        break;
                    }
                    default:
                    {
                        acc->remaining_data_packets = 0;
                        response_ready = true;
                        break;
                    }
                    }
                }
                break;
            }
            case FP_PACKET_RSP_DATA:
            {
                size_t packet_size = sizeof(rsp_data_packet) + packet_hdr->len;
                size_t size_delta = packet_size - acc->packet_buffer_index; // Bytes missing from packet
                size_t remaining_data = len - data_index;                   // Bytes available from uart block
                size_t handled_data = MIN(size_delta, remaining_data);      // Bytes handled in this iteration
                uint8_t *buffer_end = &(acc->packet_buffer[acc->packet_buffer_index]);
                uint8_t *next_data = (uint8_t *)data + data_index;
                memcpy(buffer_end, next_data, handled_data);
                data_index += handled_data;
                acc->packet_buffer_index += handled_data;
                if (acc->packet_buffer_index == packet_size)
                {
                    uint16_t checksum = get_checksum((uint8_t *)packet_hdr, packet_size - 2); //Remove 2 to account for checksum field
                    if (checksum == *(uint16_t *)(&acc->packet_buffer[acc->packet_buffer_index - 2]))
                    {
                        packet_ready = true;
                        acc->remaining_data_packets--;
                        LOG_DBG("Data packet recieved. %d remaining", acc->remaining_data_packets);
                        if (acc->remaining_data_packets == 0)
                        {
                            LOG_DBG("All data recieved");
                            response_ready = true;
                        }
                    }
                    else
                    {
                        LOG_WRN("INVALID CHECKSUM!");
                        // TODO: Something with callbacks idk
                        return true;
                    }
                }
                break;
            }
            default:
            {
                __ASSERT(false, "Unknown packet type 0x%04x.", packet_hdr->type);
                LOG_WRN("Unknown packet type");
                return true;
                break;
            }
            }
        }
        if (packet_ready)
        {
            rsp_packet *packet = (rsp_packet *)packet_hdr;
            conf->status = 0;
            if (packet->status != Success)
            {
                conf->status = -packet->status;
            }
            else
            {
                switch (packet->hdr.opcode)
                {
                case SEARCH:
                {
                    conf->status = ((uint16_t *)packet->data)[0];
                    break;
                }
                default:
                {
                    conf->status = 0;
                    break;
                }
                break;
                }
            }
            clear_accumulator(fp_dev);
        }
    }
    return response_ready;
}

/**
 * @brief Callback for uart interrupt API
 * 
 * @param uart_dev Uart device which triggered the event.
 * @param user_data Custom data. Should be a pointer to the fingerprint sensor device. 
 */
static void _sen0348_uart_irq_callback(const struct device *uart_dev, void *user_data)
{
    uart_irq_update(uart_dev);
    const struct device *fp_dev = (struct device *)user_data;
    struct sen0348_conf *conf = (struct sen0348_conf *)fp_dev->config;
    if (uart_irq_rx_ready(uart_dev))
    {
        uint8_t *buf;
        int err = k_mem_slab_alloc(conf->rx_slab, (void *)&buf, K_NO_WAIT);
        if (err)
        {
            __ASSERT(false, "RX buffer overflow");
            LOG_ERR("RX buffer overflow");
            complete_command(fp_dev);
            complete_transaction(fp_dev);
        }
        bool rsp_complete = false;
        size_t buf_size = conf->rx_slab->block_size;
        size_t buf_capacity = buf_size;
        while (1)
        {
            size_t buf_index = buf_size - buf_capacity;
            size_t bytes_read = uart_fifo_read(uart_dev, &buf[buf_index], buf_capacity);
            buf_capacity -= bytes_read;
            if (buf_capacity == 0)
            {
                rsp_complete = _accumulate_response(fp_dev, buf, bytes_read, buf_index);
                buf_capacity = buf_size;
            }
            if (bytes_read < buf_size)
            {
                rsp_complete = _accumulate_response(fp_dev, buf, bytes_read, buf_index);
                break;
            }
        }
        k_mem_slab_free(conf->rx_slab, (void *)&buf);
        if (rsp_complete)
        {
            LOG_DBG("Response completed");
            complete_command(fp_dev);
            conf->remaining_commands--;
            if (conf->remaining_commands == 0)
            {
                complete_transaction(fp_dev);
            }
        }
    }
}

/** 
 * @brief Transmits data over the uart bus connected to the fingerprint sensor.
 * @param dev Fingerprint sensor device
 * @param data Pointer to data to be transmitted
 * @param size Number of bytes to transmit
 * @return 0 on initialized transfer
 */
static int _transmit_start(const struct device *dev, uint8_t *data, size_t size)
{
    __ASSERT(size <= CONFIG_SEN0348_UART_BUFFER_SIZE,
             "Size %dB greater than configured buffer size %dB.\
             Consider increasing CONFIG_SEN0348_UART_BUFFER_SIZE in your project's KConfig file.",
             size, CONFIG_SEN0348_UART_BUFFER_SIZE);
    struct sen0348_conf *conf = (struct sen0348_conf *)dev->config;
    for (int i = 0; i < size; i++)
    {
        uart_poll_out(conf->uart_dev, data[i]);
    }
    return 0;
}

/**
 * Transmits a `get image` command to the sensor module.
 * @param dev Fingerprint sensor device
 * @return 0 on initialized command transfer 
 */
static fp_return_code _collect_image(const struct device *dev, k_timeout_t timeout)
{
    struct sen0348_conf *conf = (struct sen0348_conf *)dev->config;
    int err = start_command(dev, timeout);
    if (err)
    {
        conf->status = -EBUSY;
        return -EBUSY;
    }
    cmd_packet packet;
    init_cmd_packet(&packet, GET_IMAGE, conf->controller_id, conf->sensor_id, NULL, 0);
    err = _transmit_start(dev, (uint8_t *)&packet, sizeof(packet));
    conf->status = err ? -EBUSY : 0;
    return err ? -EBUSY : 0;
}

static int _set_led_state(const struct device *dev, fp_led_behaviour behaviour, fp_led_color start_color, fp_led_color end_color, uint8_t amount, k_timeout_t timeout)
{
    int err = start_command(dev, timeout);
    if (err)
        return -EBUSY;
    struct sen0348_conf *conf = (struct sen0348_conf *)dev->config;
    cmd_packet packet;
    uint8_t data[4] = {
        behaviour,
        start_color,
        end_color,
        amount,
    };
    init_cmd_packet(&packet, SLED_CTRL, conf->controller_id, conf->sensor_id, data, sizeof(data));
    err = _transmit_start(dev, (uint8_t *)&packet, sizeof(packet));
    return err ? -EBUSY : 0;
}

/**
 * @brief Transmits a `generate characteristics` command to the sensor module.
 * 
 * @param dev Fingerprint module device
 * @param buffer_id Ram buffer to store characteristics file in. Can be in range [0-2]
 * @return 0 on initialized command transfer, -EBUSY if transmission could not be started
 */
static int _extract_characteristics(const struct device *dev, uint16_t buffer_id, k_timeout_t timeout)
{
    __ASSERT(buffer_id == 0 || buffer_id == 1 || buffer_id == 2, "Invalid buffer id %d. Must be 0, 1, or 2.", buffer_id);
    int err = start_command(dev, timeout);
    if (err)
        return -EBUSY;
    struct sen0348_conf *conf = (struct sen0348_conf *)dev->config;
    cmd_packet packet;
    uint16_t data[1] = {
        endian16(buffer_id),
    };
    init_cmd_packet(&packet, GENERATE_CHAR, conf->sensor_id, conf->controller_id, (uint8_t *)data, sizeof(data));
    err = _transmit_start(dev, (uint8_t *)&packet, sizeof(packet));
    return err ? -EBUSY : 0;
}

static int _generate_template(const struct device *dev, uint16_t buffer_id, uint8_t image_count, k_timeout_t timeout)
{
    __ASSERT(buffer_id == 0 || buffer_id == 1 || buffer_id == 2, "Invalid buffer id %d. Must be 0, 1, or 2.", buffer_id);
    __ASSERT(image_count == 2 || image_count == 3, "Invalid image count. Only 3 and 2 is supported.");
    int err = start_command(dev, timeout);
    if (err)
        return -EBUSY;
    struct sen0348_conf *conf = (struct sen0348_conf *)dev->config;
    cmd_packet packet;
    struct template_data
    {
        uint16_t buf_id;
        uint8_t count;
    } __packed;
    struct template_data data = {
        .buf_id = endian16(buffer_id),
        .count = image_count,
    };
    init_cmd_packet(&packet, MERGE, conf->sensor_id, conf->controller_id, (uint8_t *)&data, sizeof(data));
    err = _transmit_start(dev, (uint8_t *)&packet, sizeof(packet));
    return err ? -EBUSY : 0;
}

static int _verify_charfile(const struct device *dev, uint16_t buffer_id, uint16_t template_range_start, uint16_t template_range_end, k_timeout_t timeout)
{
    int err = start_command(dev, timeout);
    if (err)
        return -EBUSY;
    struct sen0348_conf *conf = (struct sen0348_conf *)dev->config;
    cmd_packet packet;
    struct verify_charfile_data
    {
        uint16_t char_buffer_id;
        uint16_t template_range_start;
        uint16_t template_range_end;
    } __packed;
    struct verify_charfile_data data =
        {
            .char_buffer_id = endian16(buffer_id),
            .template_range_start = endian16(template_range_start),
            .template_range_end = endian16(template_range_end),
        };
    init_cmd_packet(&packet, SEARCH, conf->sensor_id, conf->controller_id, (uint8_t *)&data, sizeof(data));
    err = _transmit_start(dev, (uint8_t *)&packet, sizeof(packet));
    return err ? -EBUSY : 0;
}

static int _store_template(const struct device *dev, uint16_t buffer_id, uint16_t template_number, k_timeout_t timeout)
{
    __ASSERT(buffer_id == 0 || buffer_id == 1 || buffer_id == 2, "Invalid buffer id %d. Must be 0, 1, or 2.", buffer_id);
    int err = start_command(dev, timeout);
    if (err)
        return -EBUSY;
    struct sen0348_conf *conf = (struct sen0348_conf *)dev->config;
    cmd_packet packet;
    struct template_str_data
    {
        uint16_t template_num;
        uint16_t buf_id;
    } __packed;
    struct template_str_data data = {
        .template_num = endian16(template_number),
        .buf_id = endian16(buffer_id),
    };
    init_cmd_packet(&packet, STORE_CHAR, conf->sensor_id, conf->controller_id, (uint8_t *)&data, sizeof(data));
    err = _transmit_start(dev, (uint8_t *)&packet, sizeof(packet));
    return err ? -EBUSY : 0;
}

/**
 * @brief Adds a finger to the module database for later verification.
 * 
 * @param dev Fingerprint module device.
 * @param op_timeout Timeout for fingerprint module operations.
 * @param finger_timeout Timeout for waiting for a finger to touch the sensor.
 * @param inter_finger_sleep Waiting time between finger images.
 * @return 0 on success, -EBUSY if transaction could not be started or -EAGAIN if finger waiting timed out. 
 * Any other negative integer will be a module specific error code.
 */
static int user_enroll_finger(const struct device *dev, uint16_t id, k_timeout_t op_timeout, k_timeout_t finger_timeout, k_timeout_t inter_finger_sleep)
{
    int image_count = 3;
    if (start_transaction(dev, image_count * 3 + 2, K_NO_WAIT))
    {
        return -EBUSY;
    }
    struct sen0348_conf *conf = (struct sen0348_conf *)dev->config;
    fp_led_color led_colors[3] = {FP_LED_COLOR_BLUE, FP_LED_COLOR_CYAN, FP_LED_COLOR_GREEN};
    for (int i = 0; i < image_count; i++)
    {
        LOG_DBG("Generating char file %d", i);
        _set_led_state(dev, FP_LED_BREATHE, led_colors[i], FP_LED_COLOR_NONE, 0, op_timeout);
        k_sleep(inter_finger_sleep);
        LOG_DBG("Waiting for finger");
        if (wait_for_finger(dev, finger_timeout))
        {
            _set_led_state(dev, FP_LED_FLASH_SLOW, FP_LED_COLOR_RED, FP_LED_COLOR_RED, 2, op_timeout);
            complete_transaction(dev);
            return -EAGAIN;
        }

        LOG_DBG("Capturing image");
        _collect_image(dev, op_timeout);
        start_command(dev, op_timeout);
        if (conf->status < 0)
        {
            complete_command(dev);
            _set_led_state(dev, FP_LED_FLASH_SLOW, FP_LED_COLOR_RED, FP_LED_COLOR_RED, 2, op_timeout);
            complete_transaction(dev);
            return conf->status;
        }
        complete_command(dev);
        LOG_DBG("Extracting characteristics");
        _extract_characteristics(dev, i, op_timeout);
        start_command(dev, op_timeout);
        if (conf->status < 0)
        {
            complete_command(dev);
            _set_led_state(dev, FP_LED_FLASH_SLOW, FP_LED_COLOR_RED, FP_LED_COLOR_RED, 2, op_timeout);
            complete_transaction(dev);
            return conf->status;
        }
        complete_command(dev);
    }
    complete_command(dev);
    LOG_DBG("Generating template");
    _generate_template(dev, 0, image_count, op_timeout);
    start_command(dev, op_timeout);
    if (conf->status < 0)
    {
        complete_command(dev);
        _set_led_state(dev, FP_LED_FLASH_SLOW, FP_LED_COLOR_RED, FP_LED_COLOR_RED, 2, op_timeout);
        complete_transaction(dev);
        return conf->status;
    }
    complete_command(dev);

    LOG_DBG("Storing template");
    _store_template(dev, 0, id, op_timeout);
    start_command(dev, op_timeout);
    if (conf->status < 0)
    {
        complete_command(dev);
        _set_led_state(dev, FP_LED_FLASH_SLOW, FP_LED_COLOR_RED, FP_LED_COLOR_RED, 2, op_timeout);
        complete_transaction(dev);
        return conf->status;
    }
    complete_command(dev);
    _set_led_state(dev, FP_LED_FLASH_FAST, FP_LED_COLOR_GREEN, FP_LED_COLOR_GREEN, 2, op_timeout);
    complete_transaction(dev);
    return 0;
}

/**
 * @brief Verfies finger on the sensor against module database.
 * 
 * @param dev Fingerprint module device. 
 * @param op_timeout Timeout for fingerprint module operations.
 * @param finger_timeout Timeout for waiting for finger to placed on the sensor.
 * @return Matched finger id if positive, error if negative.
 */
static int user_verify_finger(const struct device *dev, k_timeout_t op_timeout, k_timeout_t finger_timeout)
{
    if (start_transaction(dev, 0, op_timeout))
    {
        return -EBUSY;
    }
    struct sen0348_conf *conf = (struct sen0348_conf *)dev->config;
    _set_led_state(dev, FP_LED_BREATHE, FP_LED_COLOR_GREEN, FP_LED_COLOR_BLUE, 0, op_timeout);
    if (wait_for_finger(dev, finger_timeout))
    {
        return -EAGAIN;
    }
    if (_collect_image(dev, op_timeout))
    {
        return -EBUSY;
    }
    start_command(dev, op_timeout);
    if (conf->status < 0)
    {
        complete_command(dev);
        _set_led_state(dev, FP_LED_FLASH_SLOW, FP_LED_COLOR_RED, FP_LED_COLOR_RED, 2, op_timeout);
        complete_transaction(dev);
        return conf->status;
    }
    complete_command(dev);

    if (_extract_characteristics(dev, 0, op_timeout))
    {
        return -EBUSY;
    }
    start_command(dev, op_timeout);
    if (conf->status < 0)
    {
        complete_command(dev);
        _set_led_state(dev, FP_LED_FLASH_SLOW, FP_LED_COLOR_RED, FP_LED_COLOR_RED, 2, op_timeout);
        complete_transaction(dev);
        return conf->status;
    }
    complete_command(dev);

    if (_verify_charfile(dev, 0, 1, 20, op_timeout))
    {
        return -EBUSY;
    }
    start_command(dev, op_timeout);
    if (conf->status < 0)
    {
        complete_command(dev);
        _set_led_state(dev, FP_LED_FLASH_SLOW, FP_LED_COLOR_RED, FP_LED_COLOR_RED, 2, op_timeout);
        complete_transaction(dev);
        return conf->status;
    }
    complete_command(dev);
    complete_transaction(dev);
    _set_led_state(dev, FP_LED_FLASH_FAST, FP_LED_COLOR_GREEN, FP_LED_COLOR_GREEN, 1, op_timeout);
    return conf->status;
}

/**
 * @brief Sets the baud rate of the module to match the baud rate
 * of the bus
 * 
 * @param dev sen0348 device
 * @return 0 on success, -ENOTSUP if uart bus does not support runtime baud rate configuration, and -EINVAL
 * if baudrates could not be matched.
 */
static int sync_baudrate(const struct device *dev)
{
    if (IS_ENABLED(CONFIG_SEN0348_SYNC_BAUDRATE))
    {

        if (start_transaction(dev, 0, K_NO_WAIT))
        {
            return -EBUSY;
        };
        struct sen0348_conf *conf = (struct sen0348_conf *)dev->config;
        int legal_bauds[] = {9600, 19200, 38400, 57600, 115200, 230400, 921600};
        int baud_id = -1;
        struct uart_config uart_conf;
        int err = uart_config_get(conf->uart_dev, &uart_conf);
        if (err)
            complete_transaction(dev);
        return -ENOTSUP;
        int bus_baud = uart_conf.baudrate;
        for (int i = 0; i < sizeof(legal_bauds) / sizeof(legal_bauds[0]); i++)
        {
            if (legal_bauds[i] == bus_baud)
            {
                baud_id = i + 1;
                cmd_packet packet;
                uint8_t data[6] = {
                    3,
                    baud_id,
                };
                init_cmd_packet(&packet, SET_PARAM, conf->controller_id, conf->sensor_id, data, sizeof(data));
                for (int c = 0; c < sizeof(legal_bauds) / sizeof(legal_bauds[0]); c++)
                {
                    uart_conf.baudrate = legal_bauds[c];
                    err = uart_configure(conf->uart_dev, &uart_conf);
                    if (err)
                        complete_transaction(dev);
                    return -ENOTSUP;
                    _transmit_start(dev, (uint8_t *)&packet, sizeof(packet));
                    if (start_command(dev, K_MSEC(200)))
                    {
                        complete_command(dev);
                    }
                    else
                    {
                        complete_command(dev);
                        uart_conf.baudrate = bus_baud;
                        uart_configure(conf->uart_dev, &uart_conf);
                        complete_transaction(dev);
                        return 0;
                    }
                }
            }
        }
        complete_transaction(dev);
        return -EINVAL;
    }
    return -ENOTSUP;
}

/**
 * @brief Initializes uart for a sen0348 device
 * 
 * @param dev sen0348 device
 */
static void init_uart(const struct device *dev)
{
    struct sen0348_conf *conf = (struct sen0348_conf *)dev->config;
    uart_irq_callback_user_data_set(conf->uart_dev, _sen0348_uart_irq_callback, (void *)dev);
    uart_irq_rx_enable(conf->uart_dev);
}

/**
 * @brief Initializes a sen0348 device.
 * 
 * @param dev Device to be initialized.
 */
static int sen0348_init(const struct device *dev)
{
    struct sen0348_conf *conf = (struct sen0348_conf *)dev->config;
    if (IS_ENABLED(CONFIG_SEN0348_SYNC_BAUDRATE))
    {
        sync_baudrate(dev);
    }
    if (!device_is_ready(conf->uart_dev))
    {
        LOG_ERR("UART bus %s not ready!", conf->uart_dev->name);
        return -ENODEV;
    }
    init_uart(dev);
    if (!device_is_ready(conf->irq_pin.port))
    {
        LOG_ERR("IRQ GPIO port \"%s\" not ready!", conf->irq_pin.port->name);
        return -ENODEV;
    }
    if (gpio_pin_configure(conf->irq_pin.port, conf->irq_pin.pin, GPIO_INPUT | GPIO_ACTIVE_HIGH))
    {
        return -ENODEV;
    }
    if (gpio_pin_interrupt_configure_dt(&conf->irq_pin, GPIO_INT_EDGE_TO_ACTIVE))
    {
        return -ENODEV;
    }
    gpio_init_callback(&conf->finger_gpio_callback, finger_int_handler, BIT(conf->irq_pin.pin));
    gpio_add_callback(conf->irq_pin.port, &conf->finger_gpio_callback);
    if (conf->power_pin.port != NULL && !device_is_ready(conf->power_pin.port))
    {
        LOG_ERR("POWER GPIO port \"%s\" not ready!", conf->power_pin.port->name);
        return -ENODEV;
    }
    else if (conf->power_pin.port != NULL)
    {
        if (gpio_pin_configure(conf->power_pin.port, conf->power_pin.pin, GPIO_ACTIVE_HIGH | GPIO_OUTPUT_HIGH))
        {
            return -ENODEV;
        }
    }
    return 0;
}

static struct fingerprint_api api = {
    .enroll_finger = user_enroll_finger,
    .verify_finger = user_verify_finger,
};

#define CREATE_SEN0348_DEVICE(inst)                                      \
    static K_MEM_SLAB_DEFINE(sen0348_##inst##_rx_slab,                   \
                             CONFIG_SEN0348_UART_BUFFER_SIZE,            \
                             CONFIG_SEN0348_UART_RECIEVE_BUFFER_COUNT,   \
                             UART_SLAB_ALIGNMENT);                       \
    static K_MEM_SLAB_DEFINE(sen0348_##inst##_tx_slab,                   \
                             CONFIG_SEN0348_UART_BUFFER_SIZE,            \
                             CONFIG_SEN0348_UART_TRANSMIT_BUFFER_COUNT,  \
                             UART_SLAB_ALIGNMENT);                       \
    static K_SEM_DEFINE(sen0348_transaction_sem_##inst, 1, 1);           \
    static K_SEM_DEFINE(sen0348_command_sem_##inst, 1, 1);               \
    static K_MUTEX_DEFINE(sen0348_finger_mutex_##inst);                  \
    static K_CONDVAR_DEFINE(sen0348_finger_condvar_##inst);              \
    static struct fingerprint_module_data sen0348_data_##inst = {};      \
    static struct sen0348_conf sen0348_conf_##inst = {                   \
        .rx_slab = &sen0348_##inst##_rx_slab,                            \
        .tx_slab = &sen0348_##inst##_tx_slab,                            \
        .uart_dev = DEVICE_DT_GET(DT_INST_BUS(inst)),                    \
        .irq_pin = GPIO_DT_SPEC_INST_GET(inst, int_gpios),               \
        .power_pin = GPIO_DT_SPEC_INST_GET_OR(inst, power_gpios, {{0}}), \
        .controller_id = DT_INST_PROP_OR(inst, controller_id, 0),        \
        .sensor_id = DT_INST_PROP_OR(inst, sensor_id, 0),                \
        .transaction_sem = &sen0348_transaction_sem_##inst,              \
        .cmd_sem = &sen0348_command_sem_##inst,                          \
        .finger_sync = {                                                 \
            .mut = &sen0348_finger_mutex_##inst,                         \
            .cond = &sen0348_finger_condvar_##inst,                      \
        },                                                               \
        .finger_gpio_callback = {0},                                     \
        .remaining_commands = 0,                                         \
        .status = 0,                                                     \
        .accumulator = {                                                 \
            .remaining_data_packets = 0,                                 \
            .packet_buffer = {0},                                        \
            .packet_buffer_index = 0,                                    \
        },                                                               \
    };                                                                   \
    DEVICE_DT_INST_DEFINE(inst, sen0348_init, NULL, &sen0348_data_##inst, &sen0348_conf_##inst, POST_KERNEL, SEN3048_SENSOR_INIT_PRIORITY, &api);

DT_INST_FOREACH_STATUS_OKAY(CREATE_SEN0348_DEVICE);