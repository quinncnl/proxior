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

void open_log() {
  char *log = get_file_path("log");
  config->logfd = fopen (log, "a");
}

void close_log() {
  fclose(config->logfd);
}

/* log  */

void log_error(int code, char *string, char *url, struct proxy_t *proxy) {

  time_t rawtime;
  time ( &rawtime );

  // Cannot be freed.
  char *timestr = ctime (&rawtime);
  timestr[24] = 0;

  open_log();

  if (proxy == NULL) 
    fprintf(config->logfd, "%s [%d %s] via DIRECT CONNECTION\n%s\n", timestr, code, string, url);
  else
    fprintf(config->logfd, "%s [%d %s] via %s(%s:%d)\n%s\n", timestr, code, string, proxy->name, proxy->host, proxy->port, url);

  close_log();
}
