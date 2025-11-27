#ifndef CORE_H_
#define CORE_H_

struct Fo_Args
{
    const char *caller_dir;
    const char *log_path;
    const char *build_dir;
};

void fo_watcher_tick(const char *caller_dir, const char *log_path, const char *build_dir);
void *fo_core_work(void *arg);

#endif  // CORE_H_
