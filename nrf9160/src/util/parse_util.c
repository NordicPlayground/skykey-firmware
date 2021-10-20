
#include <zephyr.h>
#include <string.h>
#include "parse_util.h"
#include <logging/log.h>


LOG_MODULE_REGISTER(parse_util, CONFIG_PARSE_UTIL_LOG_LEVEL);

#if IS_ENABLED(CONFIG_PASSWORD_MODULE)
#define ENTRY_MAX_LEN CONFIG_PASSWORD_ENTRY_NAME_MAX_LEN
#else
#define ENTRY_MAX_LEN 10
#endif

int parse_entries(const char* from_buf, char* to_buf, size_t to_buf_len, uint8_t entry_type) {
    int err = 0;
    if (entry_type > ENTRY_PASSWORD)
    {
        LOG_ERR("Entry does not exist");
        return -EINVAL;
    }
    char *rest = (char *)from_buf;
    char *line;
    char* entry;
    char platform[ENTRY_MAX_LEN];
    line = strtok_r(rest, "\n", &rest);
    
    int offset = 0;
    int entry_index = 0;
    while (line != NULL)
    {
        entry_index = 0;
        entry = strtok_r(line, "\t", &line);
        while ((entry_index != entry_type) && (entry != NULL))
        {
            entry = strtok_r(NULL, "\t", &line);
            entry_index++;
        }
        if (strlen(entry) > ENTRY_MAX_LEN)
        {
            strncpy(platform, entry, ENTRY_MAX_LEN - 3);
            strncat(platform, "...", 3);
            LOG_INF("Entry %s exceeds CONFIG_PASSWORD_ENTRY_NAME_MAX_LEN. Altered entry name: %s", entry, platform);
        }
        else
        {
            strncpy(platform, entry, ENTRY_MAX_LEN);
            LOG_DBG("Platform: %s", log_strdup(platform));
        }
        strncat(platform, "\t", 1);
        if (offset + strlen(platform) > to_buf_len)
        {
            LOG_WRN("Size of entries exceeded max buffer length. Some entries will be lost. Consider increasing CONFIG_PASSWORD_ENTRY_MAX_NUM");
            err = -ENOBUFS;
            break;
        }
        strncpy(to_buf + offset, platform, ENTRY_MAX_LEN + 1);
        offset += strlen(platform);
        line = strtok_r(NULL, "\n", &rest);
    }
    if (offset > 0)
    {
        to_buf[offset - 1] = '\0'; //remove trailing comma
    }
    return err;
}

int get_password(const char *from_buf, const char *platform, char *pw_buf, size_t pw_len) {
    int err = 0;
    char *rest = (char *)from_buf;
    char *line;
    char* entry;
    line = strtok_r(rest, "\n", &rest);
    while (line != NULL)
    {
        entry = strtok_r(line, "\t", &line);
        if (!strncmp((const char*) entry, platform, ENTRY_MAX_LEN)) {
            entry = strtok_r(NULL, "\t", &line);
            entry = strtok_r(NULL, "\t\n", &line);
            strncpy(pw_buf, entry, pw_len);
            if (strlen(entry) > pw_len) {
                err = -ERANGE;
                LOG_ERR("Parsed password entry was too long (%d). Max: %d", strlen(entry), pw_len);
                return -ERANGE;
            }
        }
        line = strtok_r(rest, "\n", &rest);
    }
    return err;
}

/**
 * @brief Separate `str` by any char in `sep` and return NULL terminated
 * sections. Consecutive `sep` chars in `str` are treated as a single
 * separator.
 *
 * @return pointer to NULL terminated string or NULL on errors.
 */
char *strtok_r(char *str, const char *sep, char **state)
{
    char *start, *end;

    start = str ? str : *state;

    /* skip leading delimiters */
    while (*start && strchr(sep, *start))
    {
        start++;
    }

    if (*start == '\0')
    {
        *state = start;
        return NULL;
    }

    /* look for token chars */
    end = start;
    while (*end && !strchr(sep, *end))
    {
        end++;
    }

    if (*end != '\0')
    {
        *end = '\0';
        *state = end + 1;
    }
    else
    {
        *state = end;
    }

    return start;
}

/**
 *
 * @brief String scanning operation
 *
 * @return pointer to 1st instance of found byte, or NULL if not found
 */

char *strchr(const char *s, int c)
{
    char tmp = (char)c;

    while ((*s != tmp) && (*s != '\0'))
    {
        s++;
    }

    return (*s == tmp) ? (char *)s : NULL;
}