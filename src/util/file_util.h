int file_write_start(void);
int file_write(const void *const fragment, size_t frag_size);
int file_extract_content(void *read_buf, size_t read_buf_size);
int file_close_and_unmount(void);