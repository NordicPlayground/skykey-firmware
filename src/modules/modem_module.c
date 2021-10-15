/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr.h>
#include <stdio.h>
#include <stdio.h>
#include <event_manager.h>

#include <modem/lte_lc.h>
#include <modem/modem_info.h>

#define MODULE modem_module

#include "modules_common.h"
#include "events/modem_module_event.h"
#include "events/cloud_module_event.h"
#include <caf/events/power_event.h>
#include <caf/events/module_state_event.h>
#include <caf/events/power_manager_event.h>


#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_MODEM_MODULE_LOG_LEVEL);

BUILD_ASSERT(!IS_ENABLED(CONFIG_LTE_AUTO_INIT_AND_CONNECT),
             "The Modem module does not support this configuration");

struct modem_msg_data
{
    union
    {
        struct cloud_module_event cloud;
        struct modem_module_event modem;
    } module;
};

/* Modem module super states. */
static enum state_type { STATE_DISCONNECTED,
                         STATE_CONNECTING,
                         STATE_CONNECTED,
                         STATE_SUSPENDED,
                         STATE_SHUTDOWN,
} state;

/* Struct that holds data from the modem information module. */
static struct modem_param_info modem_param;

/* Value that always holds the latest RSRP value. */
static uint16_t rsrp_value_latest;
const k_tid_t module_thread;

/* Modem module message queue. */
#define MODEM_QUEUE_ENTRY_COUNT 10
#define MODEM_QUEUE_BYTE_ALIGNMENT 4

K_MSGQ_DEFINE(msgq_modem, sizeof(struct modem_msg_data),
              MODEM_QUEUE_ENTRY_COUNT, MODEM_QUEUE_BYTE_ALIGNMENT);

static struct module_data self = {
    .name = "modem",
    .msg_q = &msgq_modem,
    .supports_shutdown = true,
};

/* Forward declarations. */
static void send_cell_update(uint32_t cell_id, uint32_t tac);
static void send_psm_update(int tau, int active_time);
static void send_edrx_update(float edrx, float ptw);

/* Convenience functions used in internal state handling. */
static char *state2str(enum state_type state)
{
    switch (state)
    {
    case STATE_DISCONNECTED:
        return "STATE_DISCONNECTED";
    case STATE_CONNECTING:
        return "STATE_CONNECTING";
    case STATE_CONNECTED:
        return "STATE_CONNECTED";
    case STATE_SUSPENDED:
        return "STATE_SUSPENDED";
    case STATE_SHUTDOWN:
        return "STATE_SHUTDOWN";
    default:
        return "Unknown state";
    }
}

static void state_set(enum state_type new_state)
{
    if (new_state == state)
    {
        LOG_DBG("State: %s", state2str(state));
        return;
    }

    LOG_DBG("State transition %s --> %s",
            state2str(state),
            state2str(new_state));

    state = new_state;
}

/* Handlers */
static bool event_handler(const struct event_header *eh)
{
    struct modem_msg_data msg = {0};
    bool enqueue_msg = false;

    if (is_modem_module_event(eh))
    {
        struct modem_module_event *evt = cast_modem_module_event(eh);

        msg.module.modem = *evt;
        enqueue_msg = true;
    }

    if (is_cloud_module_event(eh))
    {
        struct cloud_module_event *evt = cast_cloud_module_event(eh);

        msg.module.cloud = *evt;
        enqueue_msg = true;
    }
    if (IS_ENABLED(CONFIG_CAF_POWER_MANAGER) && is_power_down_event(eh))
    {
        int err;
        err = lte_lc_psm_req(true);
        if (err)
        {
            LOG_ERR("lte_lc_psm_req, error: %d", err);
            return err;
        }
        state_set(STATE_SUSPENDED);
        module_set_state(MODULE_STATE_OFF);
        LOG_DBG("PSM enabled");
    }

    if (IS_ENABLED(CONFIG_CAF_POWER_MANAGER) && is_wake_up_event(eh))
    {
        int err;
        err = lte_lc_psm_req(false);
        if (err)
        {
            LOG_ERR("lte_lc_psm_req, error: %d", err);
            return err;
        }
        state_set(STATE_DISCONNECTED);
        module_set_state(MODULE_STATE_READY);
        LOG_DBG("PSM disabled");
    }

    if (enqueue_msg)
    {
        int err = module_enqueue_msg(&self, &msg);

        if (err)
        {
            LOG_ERR("Message could not be enqueued");
            SEND_ERROR(modem, MODEM_EVT_ERROR, err);
        }
    }

    return false;
}

static void lte_evt_handler(const struct lte_lc_evt *const evt)
{
    switch (evt->type)
    {
    case LTE_LC_EVT_NW_REG_STATUS:
        if (evt->nw_reg_status == LTE_LC_NW_REG_UICC_FAIL)
        {
            LOG_ERR("No SIM card detected!");
            SEND_ERROR(modem, MODEM_EVT_ERROR, -ENOTSUP);
            break;
        }

        if ((evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_HOME) &&
            (evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_ROAMING))
        {
            SEND_EVENT(modem, MODEM_EVT_LTE_DISCONNECTED);
            break;
        }

        LOG_DBG("Network registration status: %s",
                evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME ? "Connected - home network" : "Connected - roaming");

        SEND_EVENT(modem, MODEM_EVT_LTE_CONNECTED);
        break;
    case LTE_LC_EVT_PSM_UPDATE:
        LOG_DBG("PSM parameter update: TAU: %d, Active time: %d",
                evt->psm_cfg.tau, evt->psm_cfg.active_time);
        send_psm_update(evt->psm_cfg.tau, evt->psm_cfg.active_time);
        break;
    case LTE_LC_EVT_EDRX_UPDATE:
    {
        char log_buf[60];
        ssize_t len;

        len = snprintf(log_buf, sizeof(log_buf),
                       "eDRX parameter update: eDRX: %f, PTW: %f",
                       evt->edrx_cfg.edrx, evt->edrx_cfg.ptw);
        if (len > 0)
        {
            LOG_DBG("%s", log_strdup(log_buf));
        }

        send_edrx_update(evt->edrx_cfg.edrx, evt->edrx_cfg.ptw);
        break;
    }
    case LTE_LC_EVT_RRC_UPDATE:
        LOG_DBG("RRC mode: %s",
                evt->rrc_mode == LTE_LC_RRC_MODE_CONNECTED ? "Connected" : "Idle");
        break;
    case LTE_LC_EVT_CELL_UPDATE:
        LOG_DBG("LTE cell changed: Cell ID: %d, Tracking area: %d",
                evt->cell.id, evt->cell.tac);
        send_cell_update(evt->cell.id, evt->cell.tac);
        break;
    default:
        break;
    }
}

static void modem_rsrp_handler(char rsrp_value)
{
    /* RSRP raw values that represent actual signal strength are
	 * 0 through 97 (per "nRF91 AT Commands" v1.1).
	 */

    if (rsrp_value > 97)
    {
        return;
    }

    /* Set temporary variable to hold RSRP value. RSRP callbacks and other
	 * data from the modem info module are retrieved separately.
	 * This temporarily saves the latest value which are sent to
	 * the Data module upon a modem data request.
	 */
    rsrp_value_latest = rsrp_value;

    LOG_DBG("Incoming RSRP status message, RSRP value is %d",
            rsrp_value_latest);
}

/* Static module functions. */
static void send_cell_update(uint32_t cell_id, uint32_t tac)
{
    struct modem_module_event *evt = new_modem_module_event();

    evt->type = MODEM_EVT_LTE_CELL_UPDATE;
    evt->data.cell.cell_id = cell_id;
    evt->data.cell.tac = tac;

    EVENT_SUBMIT(evt);
}

static void send_psm_update(int tau, int active_time)
{
    struct modem_module_event *evt = new_modem_module_event();

    evt->type = MODEM_EVT_LTE_PSM_UPDATE;
    evt->data.psm.tau = tau;
    evt->data.psm.active_time = active_time;

    EVENT_SUBMIT(evt);
}

static void send_edrx_update(float edrx, float ptw)
{
    struct modem_module_event *evt = new_modem_module_event();

    evt->type = MODEM_EVT_LTE_EDRX_UPDATE;
    evt->data.edrx.edrx = edrx;
    evt->data.edrx.ptw = ptw;

    EVENT_SUBMIT(evt);
}

static int configure_low_power(void)
{
    int err;

    err = lte_lc_psm_req(true);
    if (err)
    {
        LOG_ERR("lte_lc_psm_req, error: %d", err);
        return err;
    }

    LOG_DBG("PSM requested");

    return 0;
}

static int lte_connect(void)
{
    int err;

    err = lte_lc_connect_async(lte_evt_handler);
    if (err)
    {
        LOG_ERR("lte_lc_connect_async, error: %d", err);

        return err;
    }

    SEND_EVENT(modem, MODEM_EVT_LTE_CONNECTING);

    return 0;
}

static int modem_data_init(void)
{
    int err;

    err = modem_info_init();
    if (err)
    {
        LOG_INF("modem_info_init, error: %d", err);
        return err;
    }

    err = modem_info_params_init(&modem_param);
    if (err)
    {
        LOG_INF("modem_info_params_init, error: %d", err);
        return err;
    }

    err = modem_info_rsrp_register(modem_rsrp_handler);
    if (err)
    {
        LOG_INF("modem_info_rsrp_register, error: %d", err);
        return err;
    }

    return 0;
}

static int setup(void)
{
    int err;

    err = lte_lc_init();
    if (err)
    {
        LOG_ERR("lte_lc_init, error: %d", err);
        return err;
    }

    if (IS_ENABLED(CONFIG_MODEM_AUTO_REQUEST_POWER_SAVING_FEATURES))
    {
        err = configure_low_power();
        if (err)
        {
            LOG_ERR("configure_low_power, error: %d", err);
            return err;
        }
    }

    err = modem_data_init();
    if (err)
    {
        LOG_ERR("modem_data_init, error: %d", err);
        return err;
    }

    err = lte_connect();
    if (err)
    {
        LOG_ERR("Failed connecting to LTE, error: %d", err);
        SEND_ERROR(modem, MODEM_EVT_ERROR, err);
        return err;
    }

    //For testing purposes:
    if (IS_ENABLED(CONFIG_CAF_POWER_MANAGER)) {
        power_manager_restrict(MODULE_IDX(MODULE), POWER_MANAGER_LEVEL_SUSPENDED);
    }

    return 0;
}

/* Message handler for STATE_DISCONNECTED. */
static void on_state_disconnected(struct modem_msg_data *msg)
{
    if (IS_EVENT(msg, modem, MODEM_EVT_LTE_CONNECTED))
    {
        state_set(STATE_CONNECTED);
    }

    if (IS_EVENT(msg, modem, MODEM_EVT_LTE_CONNECTING))
    {
        state_set(STATE_CONNECTING);
    }
}

/* Message handler for STATE_CONNECTING. */
static void on_state_connecting(struct modem_msg_data *msg)
{
    if (IS_EVENT(msg, modem, MODEM_EVT_LTE_CONNECTED))
    {
        state_set(STATE_CONNECTED);
    }
}

/* Message handler for STATE_CONNECTED. */
static void on_state_connected(struct modem_msg_data *msg)
{
    if (IS_EVENT(msg, modem, MODEM_EVT_LTE_DISCONNECTED))
    {
        state_set(STATE_DISCONNECTED);
    }
}

/* Message handler for all states. */
static void on_all_states(struct modem_msg_data *msg)
{
    // if (IS_EVENT(msg, app, APP_EVT_START))
    // {
    //     int err;

    //     err = lte_connect();
    //     if (err)
    //     {
    //         LOG_ERR("Failed connecting to LTE, error: %d", err);
    //         SEND_ERROR(modem, MODEM_EVT_ERROR, err);
    //         return;
    //     }
    // }

    // if (IS_EVENT(msg, util, UTIL_EVT_SHUTDOWN_REQUEST))
    // {
    //     lte_lc_power_off();
    //     state_set(STATE_SHUTDOWN);
    //     SEND_SHUTDOWN_ACK(modem, MODEM_EVT_SHUTDOWN_READY, self.id);
    // }
}

static void module_thread_fn(void)
{
    int err;
    struct modem_msg_data msg;

    self.thread_id = k_current_get();

    err = module_start(&self);
    if (err)
    {
        LOG_ERR("Failed starting module, error: %d", err);
        SEND_ERROR(modem, MODEM_EVT_ERROR, err);
    }

    state_set(STATE_DISCONNECTED);

    err = setup();
    if (err)
    {
        LOG_ERR("Failed setting up the modem, error: %d", err);
        SEND_ERROR(modem, MODEM_EVT_ERROR, err);
    }

    while (true)
    {
        module_get_next_msg(&self, &msg, K_FOREVER);

        switch (state)
        {
        case STATE_DISCONNECTED:
            on_state_disconnected(&msg);
            break;
        case STATE_CONNECTING:
            on_state_connecting(&msg);
            break;
        case STATE_CONNECTED:
            on_state_connected(&msg);
            break;
        case STATE_SUSPENDED:
            break;
        case STATE_SHUTDOWN:
            /* The shutdown state has no transition. */
            break;
        default:
            LOG_WRN("Invalid state: %d", state);
            break;
        }

        on_all_states(&msg);
    }
}

K_THREAD_DEFINE(modem_module_thread, CONFIG_MODEM_THREAD_STACK_SIZE,
                module_thread_fn, NULL, NULL, NULL,
                K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);

EVENT_LISTENER(MODULE, event_handler);
EVENT_SUBSCRIBE_EARLY(MODULE, modem_module_event);
#if IS_ENABLED(CONFIG_CAF_POWER_MANAGER)
EVENT_SUBSCRIBE(MODULE, power_down_event);
EVENT_SUBSCRIBE(MODULE, wake_up_event);
#endif
EVENT_SUBSCRIBE(MODULE, cloud_module_event);
EVENT_SUBSCRIBE_FINAL(MODULE, util_module_event);
