#include "http.h"
#include "config.h"
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>

void signal_handler(int sig) {
  puts("signal");
}

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
  char path[50] = "/etc/proxior/";

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
    pid_t pid;
    pid = fork();

    if (pid != 0) {
      exit(EXIT_SUCCESS);
    }

    setsid();

    if ((pid = fork()) != 0) {
      exit(0);
    }

    umask(0);

    int n_desc = sysconf(_SC_OPEN_MAX), i;
    for (i = 0; i < n_desc; i++) 
      close(i); 

  }

  signal(SIGPIPE, SIG_IGN); 

  load_config(path);
  start();

  return 0;
}
