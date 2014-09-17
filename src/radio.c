#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "radio.h"

int init_watch_radio()
{
    memset(&poll_fs, 0, sizeof(poll_fs));
    poll_fs.notify_fd = inotify_init1 (IN_NONBLOCK);
    if (poll_fs.notify_fd == -1)
    {
        perror ("inotify_init1");
        return EXIT_FAILURE;
    }
    poll_fs.event_mask = IN_MOVED_TO;

    /* Inotify input */
    poll_fs.fds.fd = poll_fs.notify_fd;
    poll_fs.fds.events = POLLIN;
    poll_fs.nfds = 1;

    return EXIT_SUCCESS;
}

int add_watch_radio(char *path_to_watch)
{
    printf("add_watch_radio: %i %s\n", poll_fs.notify_fd, path_to_watch);
    if (poll_fs.wd != 0)
    {
        //TODO: 
        fprintf (stderr, "Cannot add more watches\n");
        perror ("inotify_add_watch");
        return EXIT_FAILURE;
    }
    poll_fs.wd = inotify_add_watch (poll_fs.notify_fd, path_to_watch, poll_fs.event_mask);
    if (poll_fs.wd == -1)
    {
        fprintf (stderr, "Cannot watch '%s'\n", path_to_watch);
        perror ("inotify_add_watch");
        return EXIT_FAILURE;
    }

    if (pthread_create(&poll_fs.tid, NULL, &poll_radio, NULL) != 0)
    {
        perror("can't create thread");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

void* poll_radio(void *arg)
{
    int poll_num;
    ssize_t len;

    poll_num = poll (&poll_fs.fds, poll_fs.nfds, -1);
    if (poll_num == -1)
    {
        perror ("poll");
        return; //NULL;
    }

    if (poll_num > 0)
    {
        if (poll_fs.fds.revents & POLLIN)
        {
            printf("Inotify events are available\n");
            
            /* Loop while events can be read from inotify file descriptor. */
            for (;;)
            {
                /* Read some events. */
                len = read (poll_fs.notify_fd, poll_fs.buf, sizeof poll_fs.buf);
                if (len == -1 && errno != EAGAIN)
                {
                    perror ("read");
                    return; //NULL;
                }

                /* If the nonblocking read() found no events to read, then
                 * * * * it returns -1 with errno set to EAGAIN. In that case,
                 * * * * we exit the loop. */
                if (len <= 0)
                    break;
#if 0
TODO:
                /* Loop over all events in the buffer */
                for (ptr = poll_fs.buf; ptr < poll_fs.buf + len;
                        ptr += sizeof (struct inotify_event) + poll_fs.event->len)
                {
                    poll_fs.event = (const struct inotify_event *) ptr;
                    /* Print event type */
                    if (poll_fs.event->mask & poll_fs.event_mask)
                        printf("event! %s\n", poll_fs.event->name);
                }
#else
                /* Only one event we can process */
                poll_fs.event = (struct inotify_event *) poll_fs.buf;
                printf("event! %s\n", poll_fs.event->name);
//              return poll_fs.event->name;
#endif
            }
        }
    }

//  return NULL;
}

void close_watch_radio()
{
    close (poll_fs.notify_fd);
}
