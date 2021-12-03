#ifndef _FILE_UTIL_
#define _FILE_UTIL_
#include <stdio.h>

#include <zephyr.h>
#include <device.h>
#include <fs/fs.h>
#include <fs/littlefs.h>
#include <storage/flash_map.h>

#include <kernel.h> 
#include "file_util.h"


#include <logging/log.h>
LOG_MODULE_REGISTER(file_util, CONFIG_FILE_UTIL_LOG_LEVEL);

/* Matches LFS_NAME_MAX */
#define MAX_PATH_LEN 255

static int log_contents(void);

static K_MUTEX_DEFINE(fs_mutex);

FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(storage);
static struct fs_mount_t lfs_storage_mnt = {
    .type = FS_LITTLEFS,
    .fs_data = &storage,
    .storage_dev = (void *)FLASH_AREA_ID(storage),
    .mnt_point = "/lfs",
};

struct fs_mount_t *mp = &lfs_storage_mnt;

char filename[MAX_PATH_LEN];
struct fs_file_t file;

int mount_fs(void) {
    int rc;
    rc = k_mutex_lock(&fs_mutex, K_MSEC(100));
    if (rc < 0)
    {
        LOG_WRN("Could not acquire mutex");
        return rc;
    }
    unsigned int id = (uintptr_t)mp->storage_dev;

    snprintf(filename, sizeof(filename), "%s/my_passwords", mp->mnt_point);

    rc = fs_mount(mp);
    if (rc < 0)
    {
         LOG_ERR("FAIL: mount id %u at %s: %d",
                 id, mp->mnt_point,
                 rc);
        k_mutex_unlock(&fs_mutex);
        return rc;
    }
    LOG_DBG("%s mount: %d", mp->mnt_point, rc);

    return 0;
}
/**
 *  Mounts the file system and deletes the old password file
 * @return 0 on success, on fail: negative errno.
 * */
int file_write_start(void) {
    int rc;
    rc = mount_fs();
    if (rc < 0) {
        return rc;
    }
    /* Delete the old file */
    // TODO: Find a way to set the old file as backup
    fs_unlink(filename);
    fs_file_t_init(&file);

    rc = fs_open(&file, filename, FS_O_CREATE | FS_O_APPEND);
    if (rc < 0)
    {
        LOG_ERR("FAIL: %d", rc);
    }
    return rc;
}

/**
 * Append the fragment to the password file
 * @param fragment data to append
 * @param frag_size 
 * @return 0 on success, negative errno on failure.
 * */
int file_write(const void* const fragment, size_t frag_size) {
    int rc;
    rc = fs_write(&file, fragment, frag_size);
    return rc;
}

/**
 *  Reads the bytes of a file into buffer.
 * @param read_buf buffer to put contents of password file into
 * @param read_buf_size Length of read buffer
 * @return On success: Number of bytes read. May be lower than read_buf_size 
 * if there were fewer bytes available than requested. 
 * On fail: negative errno code on error.
 * */
int file_extract_content(void *read_buf, size_t read_buf_size) {
    int rc;
    rc = mount_fs();
    if (rc < 0) {
        file_close_and_unmount();
        LOG_ERR("Failed in mounting file system: %d", rc);
        return rc;
    }

    rc = fs_open(&file, filename, FS_O_READ);
    if (rc < 0)
    {
        file_close_and_unmount();
        LOG_ERR("Failed in opening file: %d", rc);
        file_close_and_unmount();
        return rc;
    }

    rc = fs_read(&file, read_buf, read_buf_size);
    if (rc < 0) {
        file_close_and_unmount();
        LOG_ERR("Failed in reading file: %d", rc);
        file_close_and_unmount();
        return rc;
    }
    rc = file_close_and_unmount();
    return rc;
}


int file_close_and_unmount(void) {
    int rc;

    fs_close(&file);

    //log_contents(); //seems to cause stack overflow for now

    rc = fs_unmount(mp);
    k_mutex_unlock(&fs_mutex);
    return rc;
}

int log_contents(void) {
    int rc;
    struct fs_dir_t dir;

    fs_dir_t_init(&dir);

    rc = fs_opendir(&dir, mp->mnt_point);
    LOG_DBG("%s opendir: %d", mp->mnt_point, rc);

    while (rc >= 0)
    {
        struct fs_dirent ent = {0};

        rc = fs_readdir(&dir, &ent);
        if (rc < 0)
        {
            break;
        }
        if (ent.name[0] == 0)
        {
            LOG_DBG("End of files");
            break;
        }
         LOG_DBG("  %c %u %s",
                (ent.type == FS_DIR_ENTRY_FILE) ? 'F' : 'D',
                ent.size,
                ent.name);
    }

    (void)fs_closedir(&dir);
    return rc;
}

#endif /* _FILE_UTIL_ */