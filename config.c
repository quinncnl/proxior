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


#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <arpa/inet.h>

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

  return NULL;
}

static void
add_acl(struct acllist *al, char *proxy_name, char *list) {
  struct acl *acl = malloc(sizeof(struct acl));

  char *path = get_file_path(list);

  FILE *fh = fopen(path, "r");
  if (fh == NULL) {
    perror("Unable to open list file.");
    exit(1);
  }

  fseek(fh, 0L, SEEK_END);
  long s = ftell(fh);
  char *buffer;
  rewind(fh);
  buffer = malloc(s);
  if (buffer != NULL )
    fread(buffer, s, 1, fh);

  fclose(fh);

  acl->name = strdup(list);
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

static void
set_listen(char *word) {
  config->listen_addr = strdup(strtok(word, ":"));

  sscanf(strtok(NULL, ""), "%hd", &config->listen_port);

}

/* Load and resolve configuration */

void load_config(char path[]) {
  char buffer[1024];
  char word1[20], word2[20], word3[20];

  config = malloc(sizeof(conf));

  config->path = path;

  char *config_path = get_file_path("proxior.conf");
  FILE *fd = fopen(config_path, "r");

  if (fd == NULL) {
    perror("Unable to open configuration");
    exit(1);
  }

  struct proxylist *plist = malloc(sizeof(struct proxylist));
  struct acllist *alist = malloc(sizeof(struct acllist));
  plist->data = NULL;
  alist->data = NULL;

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
    else if (strcmp(word1, "listen") == 0)
      set_listen(word2);
  }
  fclose(fd);

}

void update_list(const char *list, const char *listname) {

  FILE *fh = fopen(listname, "w");
  fwrite(list, 1, strlen(list), fh);
  fclose(fh);

  //update cache
  struct acl *it = config->acl_h->data;

  while (it != NULL) {
    if (strcmp(listname, it->name) == 0) break;
    it = it->next;
  }
  assert(it != NULL);

  int newsize = strlen(list) + 1;
  if (strlen(it->data) < newsize) {
    free(it->data);
    it->data = malloc(newsize);
  }
  strcpy(it->data, list);
}

char *get_file_path(char *filename) {
  static char path[64];
  strcpy(path, config->path);
  strcat(path, filename);
  return path;
}
