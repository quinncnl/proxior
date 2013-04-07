#ifndef _CONFIG_H_
#define _CONFIG_H_

#define VERSION "1.0.1"

#include <stdio.h>
#include "hashmap.h"
#include <event2/util.h>

struct proxy_t {
  char name[20];
  char host[20];
  unsigned short port;

  enum {
    PROXY_HTTP = 0x01,
    PROXY_SOCKS = 0x02
  } type;
  
  struct proxy_t *next;
};

struct rulelist {
  char *name;
  struct hashmap_s *data;
  struct proxy_t *proxy;
  struct rulelist *next;
};

typedef struct {
  struct proxy_t *proxy_head;

  struct rulelist *rulelist_head;
  struct rulelist *rulelist_tail;

  struct proxy_t *default_proxy;
  struct proxy_t *try_proxy;
  struct timeval timeout;
  struct timeval ctimeout;

  struct hashmap_s *dnsmap;
  struct hashmap_s *trylist_hashmap;

  char *listen_addr;
  short listen_port;
  int foreground;
  char *path;
  FILE *logfd;
} config;

config cfg;
int openconn;

void load_config(char *);

void save_config();

void update_rule(char *list, char *rule);

void remove_rule(char *list, char *rule);

char *get_file_path(char *);

void flush_list() ;

void
add_to_trylist(char *url);

struct rulelist *
get_rulelist(char *listname);

#endif /* _CONFIG_H_ */
