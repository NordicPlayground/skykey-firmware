/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _MODEM_MODULE_EVENT_H_
#define _MODEM_MODULE_EVENT_H_

/**
 * @brief Modem module event
 * @defgroup modem_module_event Modem module event
 * @{
 */
#include <net/net_ip.h>
#include "event_manager.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Modem event types submitted by Modem module. */
enum modem_module_event_type
{
    MODEM_EVT_LTE_CONNECTED,
    MODEM_EVT_LTE_DISCONNECTED,
    MODEM_EVT_LTE_CONNECTING,
    MODEM_EVT_LTE_CELL_UPDATE,
    MODEM_EVT_LTE_PSM_UPDATE,
    MODEM_EVT_LTE_EDRX_UPDATE,
    MODEM_EVT_SHUTDOWN_READY,
    MODEM_EVT_ERROR,
};

/** @brief LTE cell information. */
struct modem_module_cell
{
    /** E-UTRAN cell ID. */
    uint32_t cell_id;
    /** Tracking area code. */
    uint32_t tac;
};

/** @brief PSM information. */
struct modem_module_psm
{
    /** Tracking Area Update interval [s]. -1 if the timer is disabled. */
    int tau;
    /** Active time [s]. -1 if the timer is disabled. */
    int active_time;
};

/** @brief eDRX information. */
struct modem_module_edrx
{
    /** eDRX interval value [s] */
    float edrx;
    /** Paging time window [s] */
    float ptw;
};


/** @brief Modem event. */
struct modem_module_event
{
    struct event_header header;
    enum modem_module_event_type type;
    union
    {
        struct modem_module_cell cell;
        struct modem_module_psm psm;
        struct modem_module_edrx edrx;
        /* Module ID, used when acknowledging shutdown requests. */
        uint32_t id;
        int err;
    } data;
};

EVENT_TYPE_DECLARE(modem_module_event);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _MODEM_MODULE_EVENT_H_ */
