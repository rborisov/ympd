/* debug.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
//#include "srtypes.h"
//#include "threadlib.h"
//#include "rip_manager.h"
//#include "mchar.h"
#include "ydebug.h"

#include <semaphore.h>

int vswprintf (wchar_t * ws, size_t n, const wchar_t * format, va_list arg);

/*****************************************************************************
 * Public functions
 *****************************************************************************/
#define DEBUG_BUF_LEN 2048
int debug_on = 0;
FILE* gcsfp = 0;
//static HSEM m_debug_lock;
static sem_t m_debug_lock;
static char* debug_filename = 0;
static char filename_buf[254];
static char* default_filename = "ympd.log";
static int debug_initialized = 0;

/* This is a little different from standard strncpy, because:
 *    1) behavior is known when dst & src overlap
 *    2) only copy n-1 characters max
 *    3) then add the null char
 **/
void
_strncpy (char* dst, char* src, int n)
{
    int i = 0;
    for (i = 0; i < n-1; i++) {
        if (!(dst[i] = src[i])) {
            return;
        }
    }
    dst[i] = 0;
}
void
ydebug_set_filename (char* filename)
{
    _strncpy (filename_buf, filename, 254);
    debug_filename = filename_buf;
}

void
ydebug_enable (void)
{
    debug_on = 1;
    if (!debug_filename) {
        debug_filename = default_filename;
    }
}

void
ydebug_open (void)
{
    if (!debug_on) return;
    if (!gcsfp) {
        gcsfp = fopen(debug_filename, "a");
        if (!gcsfp) {
            debug_on = 0;
        }
    }
}

void
ydebug_close (void)
{
    if (!debug_on) return;
    if (gcsfp) {
        fclose(gcsfp);
        gcsfp = 0;
    }
}

sem_t _create_sem()
{
    sem_t s;
    sem_init(&s, 0, 0);
    return s;
}
void _signal_sem(sem_t *e)
{
    if (!e)
        return;
    sem_post(&(*e));
    return;
}
void _waitfor_sem (sem_t *e)
{
    if (!e)
        return;
    sem_wait(&(*e));
    return;
}

void
ydebug_printf (char* fmt, ...)
{
    int was_open = 1;
    va_list argptr;

    if (!debug_on) {
        return;
    }

    if (!debug_initialized) {
        m_debug_lock = _create_sem();
        _signal_sem(&m_debug_lock);
    }
    _waitfor_sem (&m_debug_lock);

    va_start (argptr, fmt);
    if (!gcsfp) {
        was_open = 0;
        ydebug_open();
        if (!gcsfp) return;
    }
    if (!debug_initialized) {
        debug_initialized = 1;
        fprintf (gcsfp, "=========================\n");
        fprintf (gcsfp, "           YMPD\n");
    }

    vfprintf (gcsfp, fmt, argptr);
    fflush (gcsfp);

    va_end (argptr);
    if (!was_open) {
        ydebug_close ();
    }
    _signal_sem (&m_debug_lock);
}
