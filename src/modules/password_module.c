/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr.h>
#include <event_manager.h>

#include <device.h>
#include <fs/fs.h>
#include <fs/littlefs.h>
#include <storage/flash_map.h>
#include <stdio.h>

#include "events/display_module_event.h"
#include "events/download_module_event.h"
#include "events/password_module_event.h"

#define MODULE password_module

#include "modules_common.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_PASSWORD_MODULE_LOG_LEVEL);

struct password_msg_data
{
	union
	{
        struct display_module_event display;
		struct download_module_event download;
	} module;
};

/* Password module message queue. */
#define PASSWORD_QUEUE_ENTRY_COUNT 10
#define PASSWORD_QUEUE_BYTE_ALIGNMENT 4

K_MSGQ_DEFINE(msgq_password, sizeof(struct password_msg_data),
			  PASSWORD_QUEUE_ENTRY_COUNT, PASSWORD_QUEUE_BYTE_ALIGNMENT);


static struct module_data self = {
	.name = "password",
	.msg_q = &msgq_password,
	.supports_shutdown = true,
};

#define READ_BUF_LEN 1024 /* Maximum amount of characters read from password file */

static uint8_t read_buf[READ_BUF_LEN];
//========================================================================================
/*                                                                                      *
 *                                    Event handlers                                    *
 *                                                                                      */
//========================================================================================

/**
 * @brief Event manager event handler
 * 
 * @param eh Event header
 */
static bool event_handler(const struct event_header *eh)
{
	struct password_msg_data msg = {0};
	bool enqueue_msg = false;

	// if (is_download_module_event(eh))
	// {
	// 	struct download_module_event *evt = cast_download_module_event(eh);
	// 	msg.module.download = *evt;
	// 	enqueue_msg = true;
	// }

	if (is_display_module_event(eh))
	{
		struct display_module_event *evt = cast_display_module_event(eh);
		msg.module.display = *evt;
		enqueue_msg = true;
	}

	if (enqueue_msg)
	{
		int err = module_enqueue_msg(&self, &msg);

		if (err)
		{
			LOG_ERR("Message could not be enqueued");
			SEND_DYN_ERROR(password, PASSWORD_EVT_ERROR, err);
		}
	}

	return false;
}

/**
 * @brief Setup function for the module. Initializes modem and AWS connection.
 * 
 * @return int 0 on success, negative errno code on failure.
 */
static int setup(void)
{

	struct fs_mount_t *mp = &lfs_storage_mnt;
	unsigned int id = (uintptr_t)mp->storage_dev;
	char fname[MAX_PATH_LEN];
	struct fs_statvfs sbuf;
	const struct flash_area *pfa;
	int rc;

	snprintf(fname, sizeof(fname), "%s/boot_count", mp->mnt_point);

	rc = flash_area_open(id, &pfa);
	if (rc < 0)
	{
		printk("FAIL: unable to find flash area %u: %d\n",
			   id, rc);
		return;
	}

	printk("Area %u at 0x%x on %s for %u bytes\n",
		   id, (unsigned int)pfa->fa_off, pfa->fa_dev_name,
		   (unsigned int)pfa->fa_size);

	flash_area_close(pfa);

	rc = fs_mount(mp);
	if (rc < 0)
	{
		printk("FAIL: mount id %u at %s: %d\n",
			   (unsigned int)mp->storage_dev, mp->mnt_point,
			   rc);
		return;
	}
	printk("%s mount: %d\n", mp->mnt_point, rc);

	rc = fs_statvfs(mp->mnt_point, &sbuf);
	if (rc < 0)
	{
		printk("FAIL: statvfs: %d\n", rc);
		goto out;
	}

	printk("%s: bsize = %lu ; frsize = %lu ;"
		   " blocks = %lu ; bfree = %lu\n",
		   mp->mnt_point,
		   sbuf.f_bsize, sbuf.f_frsize,
		   sbuf.f_blocks, sbuf.f_bfree);

	struct fs_dirent dirent;

	rc = fs_stat(fname, &dirent);
	printk("%s stat: %d\n", fname, rc);
	if (rc >= 0)
	{
		printk("\tfn '%s' siz %u\n", dirent.name, dirent.size);
	}

	struct fs_file_t file;

	fs_file_t_init(&file);

	rc = fs_open(&file, fname, FS_O_READ);
	if (rc < 0)
	{
		printk("FAIL: open %s: %d\n", fname, rc);
		goto out;
	}

	rc = fs_read(&file, &read_buf, sizeof(read_buf));
	printk("%d bytes read", rc);
	rc = fs_close(&file);
	printk("%s close: %d\n", fname, rc);
	// int err;
	// err = modem_configure();
	// if (err)
	// {
	// 	LOG_ERR("modem_configure, error: %d", err);
	// 	return err;
	// }
	// err = cloud_configure();
	// if (err)
	// {
	// 	LOG_ERR("cloud_configure, error: %d", err);
	// 	return err;
	// }

out:
	rc = fs_unmount(mp);
	printk("%s unmount: %d\n", mp->mnt_point, rc);
	return 0;
}

//========================================================================================
/*                                                                                      *
 *                                     Module thread                                    *
 *                                                                                      */
//========================================================================================

static void module_thread_fn(void)
{
	LOG_DBG("Password module thread started");
	int err;
	struct password_msg_data msg;
	self.thread_id = k_current_get();

	err = module_start(&self);
	if (err)
	{
		LOG_ERR("Failed starting module, error: %d", err);
		SEND_DYN_ERROR(password, PASSWORD_EVT_ERROR, err);
	}

	err = setup();
	if (err)
	{
		LOG_ERR("setup, error %d", err);
		SEND_DYN_ERROR(password, PASSWORD_EVT_ERROR, err);
	}
	while (true)
	{

		module_get_next_msg(&self, &msg, K_FOREVER);
		if (IS_EVENT((&msg), display, DISPLAY_EVT_REQUEST_PLATFORMS)) {

		}

		// switch (state)
		// {
		// case STATE_LTE_CONNECTED:
		// 	switch (sub_state)
		// 	{
		// 	case SUB_STATE_CLOUD_CONNECTED:
		// 		on_sub_state_password_connected(&msg);
		// 		break;
		// 	case SUB_STATE_CLOUD_DISCONNECTED:
		// 		on_sub_state_cloud_disconnected(&msg);
		// 		break;
		// 	default:
		// 		LOG_ERR("Unknown Cloud module sub state");
		// 		break;
		// 	}
		// 	on_state_lte_connected(&msg);
		// 	break;
		// case STATE_LTE_DISCONNECTED:
		// 	on_state_lte_disconnected(&msg);
		// 	break;
		// case STATE_SHUTDOWN:
		// 	/* The shutdown state has no transition. */
		// 	break;
		// default:
		// 	LOG_ERR("Unknown Cloud module state.");
		// 	break;
		// }

		// on_all_states(&msg);
		// k_free(msg.data); // Free dynamic data
	}
}

K_THREAD_DEFINE(password_module_thread, CONFIG_PASSWORD_THREAD_STACK_SIZE,
				module_thread_fn, NULL, NULL, NULL,
				K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);

EVENT_LISTENER(MODULE, event_handler);
EVENT_SUBSCRIBE(MODULE, display_module_event);
EVENT_SUBSCRIBE(MODULE, download_module_event);