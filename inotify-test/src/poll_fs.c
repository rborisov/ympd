#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <string.h>

#include "poll_fs.h"

/* Read all available inotify events from the file descriptor 'fd'.
 * wd is the table of watch descriptors for the directories in argv.
 * argc is the length of wd and argv.
 * argv is the list of watched directories.
 * Entry 0 of wd and argv is unused. */

static void handle_events (int fd, int *wd, int argc, char *argv[])
{
/* Some systems cannot read integer variables if they are not
 * properly aligned. On other systems, incorrect alignment may
 * decrease performance. Hence, the buffer used for reading from
 * the inotify file descriptor should have the same alignment as
 * struct inotify_event. */

    char buf[4096]
            __attribute__ ((aligned (__alignof__ (struct inotify_event))));
    const struct inotify_event *event;
    int i;
    ssize_t len;
    char *ptr;

    /* Loop while events can be read from inotify file descriptor. */

    for (;;)
    {
            /* Read some events. */
            len = read (fd, buf, sizeof buf);
            if (len == -1 && errno != EAGAIN)
            {
                    perror ("read");
                    exit (EXIT_FAILURE);
            }

            /* If the nonblocking read() found no events to read, then
             * it returns -1 with errno set to EAGAIN. In that case,
             * we exit the loop. */

            if (len <= 0)
                    break;

            /* Loop over all events in the buffer */
            for (ptr = buf; ptr < buf + len;
                            ptr += sizeof (struct inotify_event) + event->len)
            {

                    event = (const struct inotify_event *) ptr;

                    /* Print event type */
                    if (event->mask & IN_OPEN)
                            printf ("IN_OPEN: ");
                    if (event->mask & IN_CLOSE_NOWRITE)
                            printf ("IN_CLOSE_NOWRITE: ");
                    if (event->mask & IN_CLOSE_WRITE)
                            printf ("IN_CLOSE_WRITE: ");

                    /* Print the name of the watched directory */
                    for (i = 1; i < argc; ++i)
                    {
                            if (wd[i] == event->wd)
                            {
                                    printf ("%s/", argv[i]);
                                    break;
                            }
                    }

                    /* Print the name of the file */
                    if (event->len)
                            printf ("%s", event->name);

                    /* Print type of filesystem object */
                    if (event->mask & IN_ISDIR)
                            printf (" [directory]\n");
                    else
                            printf (" [file]\n");
            }
    }
}

int radio_event()
{
//        const struct inotify_event *event;
        int i;
        ssize_t len;
        char *ptr;

        /* Loop while events can be read from inotify file descriptor. */

        for (;;)
        {
                /* Read some events. */
                len = read (poll_fs.notify_fd, poll_fs.buf, sizeof poll_fs.buf);
                if (len == -1 && errno != EAGAIN)
                {
                        perror ("read");
                        return EXIT_FAILURE;
                }

                /* If the nonblocking read() found no events to read, then
                 * * it returns -1 with errno set to EAGAIN. In that case,
                 * * we exit the loop. */
                if (len <= 0)
                        break;

                /* Loop over all events in the buffer */
                for (ptr = poll_fs.buf; ptr < poll_fs.buf + len;
                                ptr += sizeof (struct inotify_event) + poll_fs.event->len)
                {
                        poll_fs.event = (const struct inotify_event *) ptr;
                        /* Print event type */
                        if (poll_fs.event->mask & poll_fs.event_mask)
                            printf("event! %s\n", poll_fs.event->name);
                }                
        }

        return EXIT_SUCCESS;
}

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

        return EXIT_SUCCESS;
}

char *poll_song_name()
{
        int poll_num;
        poll_num = poll (&poll_fs.fds, poll_fs.nfds, -1);
        if (poll_num == -1)
        {
//                if (errno == EINTR)
//                        continue;
                perror ("poll");
//                exit (EXIT_FAILURE);
                return NULL;
        }

        if (poll_num > 0)
        {
                if (poll_fs.fds.revents & POLLIN)
                {
                        printf("Inotify events are available\n");
                        radio_event();
                }
        }
        return NULL;
}

int main (int argc, char *argv[])
{
//        char buf;
//        int fd, i, poll_num;
//    int poll_num;        
//        int *wd;
//        nfds_t nfds;
//        struct pollfd fds;

        if (argc < 2)
        {
                printf ("Usage: %s PATH [PATH ...]\n", argv[0]);
                exit (EXIT_FAILURE);
        }

        printf ("Press ENTER key to terminate.\n");
#if 0
        /* Create the file descriptor for accessing the inotify API */
        fd = inotify_init1 (IN_NONBLOCK);
        if (fd == -1)
        {
                perror ("inotify_init1");
                exit (EXIT_FAILURE);
        }
#else
        if (init_watch_radio() == EXIT_FAILURE)
                exit (EXIT_FAILURE);
#endif
#if 0
        /* Allocate memory for watch descriptors */
        wd = calloc (argc, sizeof (int));
        if (wd == NULL)
        {
                perror ("calloc");
                exit (EXIT_FAILURE);
        }

        /* Mark directories for events
         * - file was opened
         * - file was closed */
        for (i = 1; i < argc; i++)
        {
                wd[i] = inotify_add_watch (fd, argv[i], IN_OPEN | IN_CLOSE);
                if (wd[i] == -1)
                {
                        fprintf (stderr, "Cannot watch '%s'\n", argv[i]);
                        perror ("inotify_add_watch");
                        exit (EXIT_FAILURE);
                }
        }
#else
        if (add_watch_radio(argv[1]) == EXIT_FAILURE)
                exit (EXIT_FAILURE);
#endif
        /* Prepare for polling */
//        nfds = 1; //2;

        /* Console input */
//        fds[0].fd = STDIN_FILENO;
//        fds[0].events = POLLIN;

        /* Inotify input */
//        fds.fd = poll_fs.notify_fd;
//        fds.events = POLLIN;

        /* Wait for events and/or terminal input */
        printf ("Listening for events.\n");
        while (1)
        {
                poll_song_name();
#if 0
                poll_num = poll (&fds, nfds, -1);
                if (poll_num == -1)
                {
                        if (errno == EINTR)
                                continue;
                        perror ("poll");
                        exit (EXIT_FAILURE);
                }

                if (poll_num > 0)
                {
#if 0
                        if (fds[0].revents & POLLIN)
                        {
                                /* Console input is available. Empty stdin and quit */
                                while (read (STDIN_FILENO, &buf, 1) > 0 && buf != '\n')
                                        continue;
                                break;
                        }
#endif
                        if (fds.revents & POLLIN)
                        {
                                printf("Inotify events are available\n");
                                /* Inotify events are available */
//                                handle_events (poll_fs.notify_fd, poll_fs.wd, argc, argv);
                                radio_event();
                        }
                }
#endif
        }

        printf ("Listening for events stopped.\n");

        /* Close inotify file descriptor */
        close (poll_fs.notify_fd);

//        free (poll_fs.wd);
        exit (EXIT_SUCCESS);
}
