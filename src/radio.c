#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "radio.h"

pthread_mutex_t radio_lock = PTHREAD_MUTEX_INITIALIZER;

int exec_streamripper()
{
    int ret = 0;
    if (fork() == 0)
    {
        ret = system("streamripper http://stream-ru1.radioparadise.com:9000/mp3-192 -d /home/ruinrobo/Music/radio/ -o always -s");
        if (ret < 0)
            fprintf(stderr, "Cannot exec streamripper\n");
        exit(0);
    }

    return EXIT_SUCCESS;
}
int init_watch_radio()
{
    memset(&radio_cx, 0, sizeof(radio_cx));
    radio_cx.notify_fd = inotify_init1 (IN_NONBLOCK);
    if (radio_cx.notify_fd == -1)
    {
        perror ("inotify_init1");
        return EXIT_FAILURE;
    }
    radio_cx.event_mask = IN_MOVED_TO;

    /* Inotify input */
    radio_cx.fds.fd = radio_cx.notify_fd;
    radio_cx.fds.events = POLLIN;
    radio_cx.nfds = 1;

    exec_streamripper();

    return EXIT_SUCCESS;
}

int add_watch_radio(char *path_to_watch)
{
    printf("add_watch_radio: %i %s\n", radio_cx.notify_fd, path_to_watch);
    if (radio_cx.wd != 0)
    {
        //TODO: 
        fprintf (stderr, "Cannot add more watches\n");
        perror ("inotify_add_watch");
        return EXIT_FAILURE;
    }
    radio_cx.wd = inotify_add_watch (radio_cx.notify_fd, path_to_watch, radio_cx.event_mask);
    if (radio_cx.wd == -1)
    {
        fprintf (stderr, "Cannot watch '%s'\n", path_to_watch);
        perror ("inotify_add_watch");
        return EXIT_FAILURE;
    }

    if (pthread_create(&radio_cx.tid, NULL, &radio_thread, NULL) != 0)
    {
        perror("can't create thread");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

void* radio_thread(void *arg)
{
    int poll_num;
    ssize_t len;

    while (1) {
        poll_num = poll (&radio_cx.fds, radio_cx.nfds, -1);
        if (poll_num == -1)
        {
            printf("poll error\n");
            perror ("poll");
            return NULL;
        }

        if (poll_num > 0)
        {
            if (radio_cx.fds.revents & POLLIN)
            {
                printf("Inotify events are available\n");

                /* Loop while events can be read from inotify file descriptor. */
                for (;;)
                {
                    /* Read some events. */
                    len = read (radio_cx.notify_fd, radio_cx.buf, sizeof radio_cx.buf);
                    if (len == -1 && errno != EAGAIN)
                    {
                        printf("read error\n");
                        perror ("read");
                        return NULL;
                    }

                    /* If the nonblocking read() found no events to read, then
                     * * * * it returns -1 with errno set to EAGAIN. In that case,
                     * * * * we exit the loop. */
                    if (len <= 0)
                        break;
#if 0
TODO:
                    /* Loop over all events in the buffer */
                    for (ptr = radio_cx.buf; ptr < radio_cx.buf + len;
                            ptr += sizeof (struct inotify_event) + radio_cx.event->len)
                    {
                        radio_cx.event = (const struct inotify_event *) ptr;
                        /* Print event type */
                        if (radio_cx.event->mask & radio_cx.event_mask)
                            printf("event! %s\n", radio_cx.event->name);
                    }
#else
                    /* Only one event we can process */
                    pthread_mutex_lock(&radio_lock);
                    radio_cx.event = (struct inotify_event *) radio_cx.buf;
                    printf("event! %s\n", radio_cx.event->name);
                    pthread_mutex_unlock(&radio_lock);
#endif
                }
            }
        }
    }
    return NULL;
}

int radio_poll(char *outbuf)
{
    if (radio_cx.event)
    {
        printf("event!\n");
        pthread_mutex_lock(&radio_lock);
        memcpy(outbuf, radio_cx.event->name, strlen(radio_cx.event->name)+1);
        radio_cx.event = NULL;
        pthread_mutex_unlock(&radio_lock);
        return RADIO_TRUE;
    }
    return RADIO_FALSE;
}

void close_watch_radio()
{
    close (radio_cx.notify_fd);
}
