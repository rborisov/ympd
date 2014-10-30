#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include <libconfig.h>
#include "streamripper.h"

static void catch_sig (int code);
static void rip_callback (RIP_MANAGER_INFO* rmi, int message, void *data);

static BOOL         m_started = FALSE;
static BOOL         m_alldone = FALSE;
static BOOL         m_got_sig = FALSE;

RIP_MANAGER_INFO *rmi = 0;

int start_streamripper()
{
    int ret;
    STREAM_PREFS prefs;
    
    debug_set_filename("streamripper.log");
    debug_enable();

    // Load prefs
    prefs_load ();
    prefs_get_stream_prefs (&prefs, "http://stream-ru1.radioparadise.com:9000/mp3-192");
    strncpy(prefs.output_directory, "/home/ruinrobo/Music/radio", SR_MAX_PATH);
    OPT_FLAG_SET(prefs.flags, OPT_SEPARATE_DIRS, 0);
    prefs.overwrite = OVERWRITE_ALWAYS;
    prefs_save ();

//    signal (SIGINT, catch_sig);
//    signal (SIGTERM, catch_sig);

    rip_manager_init();

    /* Launch the ripping thread */
    if ((ret = rip_manager_start (&rmi, &prefs, rip_callback)) != SR_SUCCESS) {
        fprintf(stderr, "Couldn't connect to %s\n", prefs.url);
        exit(EXIT_FAILURE);
    }
    sleep(1);
    printf("rmi %d\n", rmi->started);

    return 1; //pid;
}

int stop_streamripper()
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

