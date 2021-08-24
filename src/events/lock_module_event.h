/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _LOCK_MODULE_EVENT_H_
#define _LOCK_MODULE_EVENT_H_

/**
 * @brief Util module event
 * @defgroup lock_module_event Util module event
 * @{
 */

#include "event_manager.h"

#ifdef __cplusplus
extern "C"
{
#endif

    enum lock_module_event_type
    {
        LOCK_EVT_SHUTDOWN_REQUEST
    };

    enum shutdown_reason
    {
        REASON_GENERIC,
        REASON_FOTA_UPDATE,
        REASON_TIMEOUT,
    };

    /** @brief Util event. */
    struct lock_module_event
    {
        struct event_header header;
        enum lock_module_event_type type;
        enum shutdown_reason reason;
    };

    EVENT_TYPE_DECLARE(lock_module_event);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _LOCK_MODULE_EVENT_H_ */
