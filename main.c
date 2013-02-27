#include "http.h"
#include "config.h"
#include <unistd.h>
#include <sys/stat.h>

static void
show_help() {
  char *help = 
    "-d    Run as a daemon.\n"
    "-p    Set configuration path.\n"
    "\n";
  fputs(help, stderr);
}

int main(int argc, char *argv[])
{

  int c, daemon = 0;
  char path[50] = "./";

  while ((c = getopt(argc, argv, "dp:")) != -1) {
    switch (c) {
    case 'd' :
      daemon = 1;
      break;
    case 'p' :
      strcpy(path, optarg);
      break;
    default :
      show_help();
      exit(EXIT_SUCCESS);
    }
  }

  if (daemon) {

    if (fork() != 0) {
      exit(EXIT_SUCCESS);
    }

    setsid();

    if (fork() != 0) 
      exit(0);

    umask (0177);

    close(0);
    close(1);
    close(2);

  }
  printf("Path: %s\n", path);

  load_config(path);
  start();

  return 0;
}
