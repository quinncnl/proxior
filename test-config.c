#include "config.h"

int main(int argc, char *argv[])
{
  load_config();
  /*
  struct proxylist *proxy = config->proxy_h;
  struct proxy_t *node = proxy->data;
  do {
    printf("proxy name: %s host:port: %s:%d \n", node ->name, node->host, node->port);
    node = node->next;
  } while (node != NULL);
  */

  struct acllist *al = config->acl_h;
  struct acl *node = al->data;
  do {
    printf("name: %s\ndata: \n%s", node->proxy->name, node->data);
    node = node->next;
  } while (node != NULL);
  return 0;
}
