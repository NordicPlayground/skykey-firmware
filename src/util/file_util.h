int file_write_start(void);
int file_write(const void *const fragment, size_t frag_size, bool final_fragment);
int extract_contents(void *read_buf, size_t read_buf_size);