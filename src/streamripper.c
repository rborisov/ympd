#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include <libconfig.h>
#include "streamripper.h"

//static void catch_sig (int code);
static void rip_callback (RIP_MANAGER_INFO* rmi, int message, void *data);

static BOOL         m_started = FALSE;
static BOOL         m_alldone = FALSE;
static BOOL         m_got_sig = FALSE;

RIP_MANAGER_INFO *rmi = 0;

pid_t sr_start()
{
    int ret;
    pid_t pid = fork();
    STREAM_PREFS prefs;

    strncpy (prefs.url, "http://stream-ru1.radioparadise.com:9000/mp3-192", MAX_URL_LEN);

    // Load prefs
//    prefs_load ();
//    prefs_get_stream_prefs (&prefs, url);
//    prefs_save ();

//    signal (SIGINT, catch_sig);
//    signal (SIGTERM, catch_sig);

    rip_manager_init();

    if (pid == 0) {
        signal(SIGCHLD, SIG_DFL);
        /* Launch the ripping thread */
        if ((ret = rip_manager_start (&rmi, &prefs, rip_callback)) != SR_SUCCESS) {
            fprintf(stderr, "Couldn't connect to %s\n", prefs.url);
            exit(EXIT_FAILURE);
        }
        printf("rmi %d\n", rmi->started);
    }
//    printf("rmi %d\n", rmi->started);

    return pid;
}

int sr_stop()
{
    rip_manager_stop (rmi);
    rip_manager_cleanup ();

    return 0;
}

void catch_sig(int code)
{
//    print_to_console ("\n");
    if (!m_started)
        exit(2);
    m_got_sig = TRUE;
}

/*
 * This will get called whenever anything interesting happens with the
 * stream. Interesting are progress updates, error's, when the rip
 * thread stops (RM_DONE) starts (RM_START) and when we get a new track.
 *
 * for the most part this function just checks what kind of message we got
 * and prints out stuff to the screen.
 */
void rip_callback (RIP_MANAGER_INFO* rmi, int message, void *data)
{
    ERROR_INFO *err;
    switch(message)
    {
        case RM_UPDATE:
//            print_status (rmi);
            break;
        case RM_ERROR:
            err = (ERROR_INFO*)data;
            fprintf(stderr, "\nerror %d [%s]\n", err->error_code, err->error_str);
            m_alldone = TRUE;
            break;
        case RM_DONE:
//            print_to_console ("bye..\n");
            m_alldone = TRUE;
            break;
        case RM_NEW_TRACK:
//            print_to_console ("\n");
            break;
        case RM_STARTED:
            m_started = TRUE;
            break;
    }
}

