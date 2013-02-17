#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <stdio.h>
#include "hashmap.h"
#include <event2/util.h>

struct proxy_t {
  char name[20];
  char host[20];
  int port;
  struct proxy_t *next;
};

struct proxylist {
  struct proxy_t *data;
  int count;
};

struct acl {
  char *name;
  struct hashmap_s *data;
  struct proxy_t *proxy;
  struct acl *next;
};

struct acllist {
  struct acl *data;
  int count;
};

typedef struct {
  struct proxylist *proxy_h;
  struct acllist *acl_h;
  struct proxy_t *default_proxy;
  struct proxy_t *try_proxy;
  struct timeval timeout;
  char *listen_addr;
  short listen_port;
  int foreground;
  char *path;
  FILE *logfd;
} conf;

conf *config;

void load_config(char *);

void save_config();

void update_list(const char *list, const char *listname) ;

char *get_file_path(char *);

//void free_config();

#endif /* _CONFIG_H_ */
