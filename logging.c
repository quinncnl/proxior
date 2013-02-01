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
