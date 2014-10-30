/* ympd
   (c) 2013-2014 Andrew Karpow <andy@ndyk.de>
   This project's homepage is: http://www.ympd.org
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/time.h>
#include <pthread.h>
#include <libconfig.h>
#include <pwd.h>

#include "streamripper.h"

int force_exit = 0;

void bye()
{
    force_exit = 1;
}

int main(int argc, char **argv)
{
    char filename[512] = "";
    int n, option_index = 0;
    unsigned int current_timer = 0, last_timer = 0;

    start_streamripper();
    printf("start_streamripper\n");

    while (!force_exit) {
        if (poll_streamripper(filename))
            printf("%s\n", filename);
    }

    stop_streamripper();

    return EXIT_SUCCESS;
}
