#include <stdlib.h>
#include <stdio.h>
#include <error.h>
#include <errno.h>
#include <sys/inotify.h>
#include <limits.h>
#include <unistd.h>

#include "fswatch.h"

int init_fswatch(char *pathtowatch)
{
    int wd;
    int rc = EXIT_SUCCESS;

    fswatch.inotifyFd = inotify_init();
    if (fswatch.inotifyFd == -1)
    {
        fprintf(stderr, "inotify_init\n");   
        return EXIT_FAILURE;
    }

    /* For each command-line argument, add a watch for all events */

    wd = inotify_add_watch(fswatch.inotifyFd, pathtowatch, IN_ALL_EVENTS);
    if (wd == -1) {
        fprintf(stderr, "inotify_add_watch\n");
        return EXIT_FAILURE;
    }
    printf("Watching %s using wd %d\n", pathtowatch, wd);
    
    return rc;
}

int read_events()
{
    int rc = EXIT_SUCCESS;
    ssize_t numRead;
    char *p;

    numRead = read(fswatch.inotifyFd, fswatch.buf, BUF_LEN);
    if (numRead == 0) {
        fprintf(stderr, "read() from inotify fd returned 0!\n");
        printf("err1\n");
        return EXIT_FAILURE;
    }

    if (numRead == -1) {
        fprintf(stderr, "read\n");
        printf("err2\n");
        return EXIT_FAILURE;
    }

    printf("Read %ld bytes from inotify fd\n", (long) numRead);

    /* Process all of the events in buffer returned by read() */
    for (p = fswatch.buf; p < fswatch.buf + numRead; ) {
        fswatch.event = (struct inotify_event *) p;
//        displayInotifyEvent(event);

        p += sizeof(struct inotify_event) + fswatch.event->len;
    }


    return rc;
}
