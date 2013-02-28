#ifndef _HTTP_H_
#define _HTTP_H_

#include "common.h"
#include "logging.h"
#include "config.h"

extern config cfg;

typedef struct conn
{
  struct bufferevent *be_client, *be_server;
  char url[MAX_URL_LEN];
  struct parsed_url *purl;
  char method[10];
  char version[10];

  // type of service
  enum {
    CONN_DIRECT = 0x01,
    CONN_HTTP_PROXY = 0x02,
    CONN_SOCKS_PROXY = 0x03,
    CONN_CONFIG = 0x08
  } tos;

  /* proxy to use, NULL for direct */
  struct proxy_t *proxy;

  // For CONNECT method
  short handshaked;

  /*  Used to track current http state */
  struct state *state; 

} conn_t;


struct state {
  /* for content section */
  int length, read; 
  
  enum {
    STATE_REQ_LINE, //req line to be read
    STATE_HEADER, 
    STATE_BODY
  } pos;

  /* Request line sent */
  int req_sent;

  char *body;

};

void
free_server(conn_t *conn);

void
free_conn(conn_t *conn);

void
http_ready_cb(int (*callback)(void *ctx), void *ctx);

void
read_server(struct bufferevent *bev, void *ctx);

void 
server_event(struct bufferevent *bev, short e, void *ptr);

void
error_msg(struct evbuffer *output, char *msg);

void server_connected(conn_t *conn) ;

void start();

#endif /* _HTTP_H_ */
