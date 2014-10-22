#ifndef __RADIO_H__
#define __RADIO_H__

#include <poll.h>
#include <sys/inotify.h>
#include <pthread.h>

#define RADIO_TRUE 1
#define RADIO_FALSE 0

struct t_radio_cx {
        int notify_fd;
        int wd;
        char buf[4096]
                __attribute__ ((aligned (__alignof__ (struct inotify_event))));
        int event_mask;
        struct pollfd fds;
        nfds_t nfds;
        struct inotify_event *event;
        pthread_t tid;
} radio_cx;

int init_watch_radio(char* url);
int add_watch_radio(char *path_to_watch);
void *radio_thread(void* arg);
int radio_poll(char* outbuf);
void close_watch_radio();

#endif
