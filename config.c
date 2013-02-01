#include "config.h"
#include <stdlib.h>
#include <string.h>

static void
add_proxy(struct proxylist *pl, char *name, char *ap) {
  struct proxy_t *proxy = malloc(sizeof(struct proxy_t));
  proxy->next = pl->data;
  strcpy(proxy->name, name);
  strcpy(proxy->host, strtok(ap, ":"));
  proxy->port = atoi(strtok(NULL, ""));
  pl->data = proxy;
  pl->count++;
}

static struct proxy_t *
find_proxy(char *proxy_name) {
  struct proxylist *proxy = config->proxy_h;
  struct proxy_t *node = proxy->data;
  do {
    if(strcmp(node->name, proxy_name) == 0)
      return node;
    node = node->next;
  } while (node != NULL);
  
  if (strcmp(proxy_name, "direct") == 0)
    return NULL;

  perror("proxy not found, use direct\n");
  return NULL;
}

static void
add_acl(struct acllist *al, char *proxy_name, char *list) {
  struct acl *acl = malloc(sizeof(struct acl));

  FILE *fh = fopen(list, "r");
  //assume ( fh != NULL );

  fseek(fh, 0L, SEEK_END);
  long s = ftell(fh);
  char *buffer;
  rewind(fh);
  buffer = malloc(s);
  if (buffer != NULL )
    fread(buffer, s, 1, fh);
  fclose(fh);

  acl->proxy = find_proxy(proxy_name);
  acl->data = buffer;
  acl->next = al->data;
  al->data = acl;
  al->count++;
}

static void 
set_default_proxy(char *proxy_name) {
  config->default_proxy = find_proxy(proxy_name);
}

static void
set_timeout(char *time) {
  config->timeout.tv_sec = atoi(time);
  config->timeout.tv_usec = 0;
}

/* Load and resolve configuration */

void load_config() {
  char buffer[1024];
  char word1[20], word2[20], word3[20];
  FILE *fd = fopen("proxyrouter.conf", "r");

  struct proxylist *plist = malloc(sizeof(struct proxylist));
  struct acllist *alist = malloc(sizeof(struct acllist));
  plist->data = NULL;
  alist->data = NULL;

  config = malloc(sizeof(conf));
  config->proxy_h = plist;
  config->acl_h = alist;

  while(fgets(buffer, sizeof(buffer), fd)) {
    if (buffer[0] == '#'|| buffer[0] == '\n') continue;
    sscanf(buffer, "%s %s %s", word1, word2, word3);
    if (strcmp(word1, "proxy") == 0) {
      add_proxy(plist, word2, word3);
    }
    else if (strcmp(word1, "acl") == 0) {
      add_acl(alist, word2, word3);

    }
    else if (strcmp(word1, "acl-default") == 0) {
      set_default_proxy(word2);
    }
    else if (strcmp(word1, "timeout") == 0) 
      set_timeout(word2);
  }
  fclose(fd);

}

/*
void free_config() {
  struct proxy_t *node_p = config->proxy_h;
  struct proxy_t *next;
  while ((next = node_p->next) != NULL) {
    free(node_p);
    node_p = next;
  }

  free(config);
}

*/
