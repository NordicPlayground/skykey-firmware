enum entry_type
{
    ENTRY_ACCOUNT,
    ENTRY_LOGIN_NAME,
    ENTRY_PASSWORD,
};

int parse_entries(const char *from_buf, char *to_buf, size_t to_buf_len, uint8_t entry_type);
int get_password(const char* from_buf, const char* platform, char* pw_buf, size_t pw_len);

char *strtok_r(char *str, const char *sep, char **state);
char *strchr(const char *s, int c);