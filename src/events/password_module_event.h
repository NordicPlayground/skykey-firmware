/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _PASSWORD_MODULE_EVENT_H_
#define _PASSWORD_MODULE_EVENT_H_

/**
 * @brief Password module event
 * @defgroup password_module_event Password module event
 * @{
 */

#include "event_manager.h"

#ifdef __cplusplus
extern "C"
{
#endif

	/** @brief Password event types submitted by Password module. */
	enum password_module_event_type
	{
		PASSWORD_EVT_REQ_DOWNLOAD,
		PASSWORD_EVT_ERROR,
	};

	/**
	 * @brief Callback for handling downloaded file fragments.
	 * 
	 * @param data Pointer to file fragment.
	 * @param frag_size Size of current fragment.
	 * @param file_size Total size of file being downloaded.
	 * 
	 * @return Negative error code when download should be cancelled, 0 or positive otherwise.
	 */
	typedef int (*password_download_cb_t)(const void* const data, size_t frag_size, size_t file_size);

    struct password_req_download_params {
        int64_t version;
		password_download_cb_t callback;
    }; 


	/** @brief Password event. */
	struct password_module_event
	{
		struct event_header header;

		union
		{
            struct password_req_download_params download_params;
			/* Module ID, used when acknowledging shutdown requests. */
			uint32_t id;
			int err;
		} data;

		enum password_module_event_type type;
		struct event_dyndata dyndata;
	};

	EVENT_TYPE_DYNDATA_DECLARE(password_module_event);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _PASSWORD_MODULE_EVENT_H_ */
