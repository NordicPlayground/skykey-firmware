/** @brief File on which to perform operations*/
enum file_selection
{
    FILE_PASSWORD,
    FILE_TIMEOUT
};

int file_write_start(void);
int file_write(const void *const fragment, size_t frag_size);
int file_extract_content(void *read_buf, size_t read_buf_size);
int file_close_and_unmount(void);