#ifndef __POLL_FS_H__
#define __POLL_FS_H__

struct t_poll_fs {
        int notify_fd;
        int wd;
        char buf[4096]
                __attribute__ ((aligned (__alignof__ (struct inotify_event))));
        int event_mask;
        struct pollfd fds;
        nfds_t nfds;
        const struct inotify_event *event;
} poll_fs;

#endif
