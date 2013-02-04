#include "http.h"
#include "config.h"
#include <signal.h>

int main(int argc, char *argv[])
{

  //  signal(SIGPIPE, SIG_IGN); 

  load_config();
  open_log();
  start();
  close_log();
  return 0;
}
