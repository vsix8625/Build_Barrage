#ifndef UTIL_H_
#define UTIL_H_

#define BARR_LOCAL_SHARE_DATA_DIR ".local/share/barr"

#define LOG_VA_CHECK(fmtargnumber) __attribute__((format(__printf__, fmtargnumber, fmtargnumber + 1)))

char *time_str(void);
const char *fo_getcwd(void);
char *gtvalue(const char *fpath, const char *key);

bool is_dir(const char *dir);
bool is_file(const char *path);
int rmrf(const char *path);

int flwrite(const char *filename, const char *format, ...) LOG_VA_CHECK(2);
int flappend(const char *filename, const char *format, ...) LOG_VA_CHECK(2);

void fo_notify(const char *summary, const char *body, const char *urgency, const char *icon);
int run_process_blocking(const char *name, char **args);
bool is_blank(const char *s);

#endif  // UTIL_H_
