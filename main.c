#include "http.h"
#include "config.h"

int main(int argc, char *argv[])
{

  load_config();
  open_log();
  start();
  close_log();
  return 0;
}
