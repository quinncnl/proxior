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

struct rulelist *
get_rulelist(char *listname) {
  struct rulelist *rl = cfg.rulelist_head;
  while (rl != NULL) {
    if (strcmp(rl->name, listname) == 0) 
      return rl;
    else
      rl = rl->next;
  }
  return NULL;
}

void
add_to_trylist(char *dom) {

  struct rulelist *rl = get_rulelist("trylist");
  struct hashmap_s *map  = rl->data;

  if (map == NULL)
    return;

  hashmap_insert(map, dom);
}

static void
add_proxy(char *name, char *ap, unsigned char type) 
{
  struct proxy_t *proxy = malloc(sizeof(struct proxy_t));

  strcpy(proxy->name, name);
  strcpy(proxy->host, strtok(ap, ":"));
  proxy->port = atoi(strtok(NULL, ""));

  proxy->type = type;
  proxy->next = cfg.proxy_head;
  cfg.proxy_head = proxy;

}

static struct proxy_t *
find_proxy(char *proxy_name) {

  struct proxy_t *node = cfg.proxy_head;
  do {
    if(strcmp(node->name, proxy_name) == 0)
      return node;
    node = node->next;
  } while (node != NULL);
  
  if (strcmp(proxy_name, "direct") == 0)
    return NULL;

  fprintf(stderr, "Cannot find proxy '%s'. Exiting\n", proxy_name);

  exit(1);

}

static void
add_acl(char *proxy_name, char *list) {
  struct rulelist *rl = malloc(sizeof(struct rulelist));
  struct hashmap_s *map;

  if (strcmp(list, "trylist") == 0) {
    map = hashmap_create(20);
    goto add;
  }

  char *path = get_file_path(list);

  FILE *fh = fopen(path, "r");
  char *buf = malloc(200);

  if (fh == NULL) {
    perror("Unable to open list file.");
    exit(1);
  }

  int lines = get_line_count(fh);

  printf("Rules in %s: %d\n", list, lines);

  map = hashmap_create(lines + 10);

  while (fgets(buf, 200, fh)) {
    // '\n' included
    int len = strlen(buf) - 1;

    if (len == 0) break;

    if(buf[len] == '\n') 
      buf[len] = 0;

    hashmap_insert(map, buf);
  }    
  free(buf);
  fclose(fh);

 add:

  rl->name = strdup(list);
  rl->proxy = find_proxy(proxy_name);
  rl->data = map;
  if (cfg.rulelist_head == NULL)
    cfg.rulelist_head = rl;
  else 
    cfg.rulelist_tail->next = rl;

  rl->next = NULL;
  cfg.rulelist_tail = rl;

}

static void 
set_default_proxy(char *proxy_name) {
  cfg.default_proxy = find_proxy(proxy_name);
}

static void 
set_try_proxy(char *proxy_name) {
  cfg.try_proxy = find_proxy(proxy_name);
}

static void
set_timeout(char *time) {
  cfg.timeout.tv_sec = atoi(time);
  cfg.timeout.tv_usec = 0;
}

static void
set_listen(char *word) {
  cfg.listen_addr = strdup(strtok(word, ":"));

  sscanf(strtok(NULL, ""), "%hd", &cfg.listen_port);

}

/* Load and resolve configuration */

void load_config(char path[]) 
{
  char buffer[1024];
  char word1[20], word2[20], word3[20];

  memset(&cfg, 0, sizeof(cfg));

  cfg.path = path;

  char *config_path = get_file_path("proxior.conf");
  FILE *fd = fopen(config_path, "r");

  if (fd == NULL) {
    perror("Unable to open configuration");
    exit(1);
  }

  while(fgets(buffer, sizeof(buffer), fd)) {
    if (buffer[0] == '#' || buffer[0] == '\n') 
      continue;

    sscanf(buffer, "%s %s %s", word1, word2, word3);

    if (strcmp(word1, "proxy") == 0) 
      add_proxy(word2, word3, PROXY_HTTP);

    else if (strcmp(word1, "socks5") == 0)
      add_proxy(word2, word3, PROXY_SOCKS);

    else if (strcmp(word1, "acl") == 0)
      add_acl(word2, word3);

    else if (strcmp(word1, "acl-default") == 0) 
      set_default_proxy(word2);

    else if (strcmp(word1, "acl-try") == 0) {
      set_try_proxy(word2);
      add_acl(word2, "trylist");
    }
    
    else if (strcmp(word1, "timeout") == 0) 
      set_timeout(word2);
    else if (strcmp(word1, "listen") == 0)
      set_listen(word2);
    else {
      fprintf(stderr, "Invalid command %s. Ignored.\n", word1);
    }
  }
  fclose(fd);


  cfg.ctimeout.tv_sec = 5;
  cfg.ctimeout.tv_usec = 0;

}

/* Update single URL rule */

void update_rule(char *list, char *rule)
{

  /* Remove existing rule */

  struct rulelist *it = cfg.rulelist_head;

  while (it != NULL) {

    hashmap_remove(it->data, rule);

    if (strcmp(list, it->name) == 0) 
      hashmap_insert(it->data, rule);

    it = it->next;
  }

}

void remove_rule(char *list, char *rule) 
{
  struct rulelist *it = cfg.rulelist_head;

  while (it != NULL) {

    if (strcmp(list, it->name) == 0) 
      hashmap_remove(it->data, rule);

    it = it->next;
  }
}

char *get_file_path(char *filename) 
{
  static char path[64];

  strcpy(path, cfg.path);
  strcat(path, filename);
  return path;
}

/* Flush all lists to disk */

void flush_list() 
{
  struct rulelist *rl = cfg.rulelist_head;
  struct hashmap_s *map;
  FILE *fd;
  struct hashentry_s *it;
  int i;

  while (rl) {
    map = rl->data;
    fd = fopen(get_file_path(rl->name), "w");

    for (i = 0; i < map->size; i++) {
      
      it = map->buckets[i].head;
      while (it) {
	fprintf(fd, "%s\n", it->data);
	it = it->next;
      }
    }
    fclose(fd);
    rl = rl->next;
  }

}
