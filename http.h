#ifndef _HTTP_H_
#define _HTTP_H_

#include "common.h"
#include "logging.h"
#include "config.h"

extern conf *config;

typedef struct conn
{
  struct bufferevent *be_client, *be_server;
  char url[MAX_URL_LEN];
  char method[10];
  char ver[10];
  struct proxy_t *proxy;
  int pos;
  int config;
  void *rpc;
} conn_t;

#define CONNECT_DIRECT 1
#define CONNECT_HTTP_PROXY 2

void start();

#endif /* _HTTP_H_ */
