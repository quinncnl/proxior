#include "util.h"

int main(int argc, char *argv[])
{
  char url[200] = "http://www.google.com:33/";
  char *domain; int port;
  domain = simple_parse_url(url, &port);
  printf("domain: %s, port: %d\n", domain, port);

  return 0;
}
