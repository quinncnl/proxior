/*

  Copyright (c) 2013 by Clear Tsai

  Proxior is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  any later version.

  Proxior is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Foobar.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <stdio.h>
#include <time.h>
#include "logging.h"

FILE *fd;

void open_log() {
  fd = fopen ("log", "a");
}

void close_log() {
  fclose(fd);
}

/* log  */

void log_reset(char *url) {
  time_t rawtime;
  time ( &rawtime );
  char *timestr = ctime (&rawtime);
  timestr[24] = 0;
  fputs(timestr, fd);
  fputs(" 502 ", fd);
  fputs(url, fd);
  fputc('\n', fd);

  fflush(fd);
}
