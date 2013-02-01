#include "match.h"
#include "config.h"

int main(int argc, char *argv[])
{
  load_config();
  struct proxy_t *proxy = match_list(argv[1]);

  printf("Use proxy: %s:%d\n", proxy->host, proxy->port);

  return 0;
}
