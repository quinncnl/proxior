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
#include "util.h"
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
  char *buf = malloc(200);

  if (fh == NULL) {
    perror("Unable to open list file.");
    exit(1);
  }

  struct hashmap_s *map = hashmap_create(101);

  while (fgets(buf, 200, fh)) {
    int len = strlen(buf) - 1;

    if (len == -1) break;

    if(buf[len] == '\n') 
      buf[len] = 0;

    hashmap_insert(map, buf);
  }    
  free(buf);
  fclose(fh);

  acl->name = strdup(list);
  acl->proxy = find_proxy(proxy_name);
  acl->data = map;
  acl->next = al->data;
  al->data = acl;
  al->count++;
}

static void 
set_default_proxy(char *proxy_name) {
  config->default_proxy = find_proxy(proxy_name);
}

static void 
set_try_proxy(char *proxy_name) {
  config->try_proxy = find_proxy(proxy_name);
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
    else if (strcmp(word1, "acl") == 0)
      add_acl(alist, word2, word3);

    else if (strcmp(word1, "acl-default") == 0) 
      set_default_proxy(word2);

    else if (strcmp(word1, "acl-try") == 0) 
      set_try_proxy(word2);
    
    else if (strcmp(word1, "timeout") == 0) 
      set_timeout(word2);
    else if (strcmp(word1, "listen") == 0)
      set_listen(word2);
  }
  fclose(fd);

}

/* Update single URL rule */

void update_rule(char *list, char *rule)
{

  /* Remove existing rule */

  struct acl *it = config->acl_h->data;

  while (it != NULL) {

    hashmap_remove(it->data, rule);

    if (strcmp(list, it->name) == 0) 
      hashmap_insert(it->data, rule);

    it = it->next;
  }

}

void remove_rule(char *list, char *rule) 
{
  struct acl *it = config->acl_h->data;

  while (it != NULL) {

    if (strcmp(list, it->name) == 0) 
      hashmap_remove(it->data, rule);

    it = it->next;
  }
}

char *get_file_path(char *filename) 
{
  static char path[64];
  strcpy(path, config->path);
  strcat(path, filename);
  return path;
}

/* Flush all lists to disk */

void flush_list() 
{
  struct acl *acl = config->acl_h->data;
  struct hashmap_s *map;
  FILE *fd;
  struct hashentry_s *it;
  int i;

  while (acl) {
    map = acl->data;
    fd = fopen(get_file_path(acl->name), "w");

    for (i = 0; i < map->size; i++) {
      
      it = map->buckets[i].head;
      while (it) {
	puts(it->data);
	fprintf(fd, "%s\n", it->data);
	it = it->next;
      }
    }
    fclose(fd);
    acl = acl->next;
  }

}
