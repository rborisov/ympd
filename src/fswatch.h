#ifndef __FSWATCH_H__
#define __FSWATCH_H__

#include <sys/inotify.h>
#include <limits.h>

#define BUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))

struct t_fswatch {
    char buf[BUF_LEN] __attribute__ ((aligned(8)));
    int inotifyFd;
    struct inotify_event *event;
} fswatch;

int init_fswatch(char*);
int read_events();

#endif
