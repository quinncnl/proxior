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
  along with Proxior.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "common.h"
#include "socks.h"
#include "http.h"

static void 
socks_event(struct bufferevent *bev, short e, void *ptr) 
{
  conn_t *conn = ((struct socksctx *)ptr)->ctx;

  if (e & (BEV_EVENT_ERROR|BEV_EVENT_EOF|BEV_EVENT_TIMEOUT)) {

    if (e & BEV_EVENT_ERROR) {
      int code = evutil_socket_geterror(bufferevent_getfd(conn->be_server)); 

      fprintf(stderr, "Error connecting to socks server: %s\n", evutil_socket_error_to_string(code));

      error_msg(bufferevent_get_output(conn->be_client), "Error connecting to socks server.");

      free_conn(conn);
      return;
    }
  }    
}

static void
socks_read(struct bufferevent *bev, void *ptr) {
  struct socksctx *ctx = ptr;

  if (ctx->phase == 0) {
    char hs1[2];
    bufferevent_read(bev, hs1, 2);

    if (hs1[0] != 0x05 || hs1[1] != 0x00) 
      return ;

    unsigned char req[6 + 256];

    req[0] = 0x05;
    req[1] = 0x01;
    req[2] = 0x00;
    req[3] = 0x03;
    req[4] = strlen(ctx->host);

    memcpy(&req[5], ctx->host, req[4]);

    unsigned short netport =  htons(ctx->port);
    memcpy(&req[5 + req[4]], &netport, 2);

    bufferevent_write(bev, req, 7 + req[4]);

    ctx->phase = 1;

  }
  else if (ctx->phase == 1) {

    char hs2[2];
    conn_t * conn = ctx->ctx;

    bufferevent_read(bev, hs2, 2);

    if (hs2[1]) {
      error_msg(bufferevent_get_output(conn->be_client), "Cannot connect to socks server");
      free(ctx);
      free_server(conn);
      return;
    }

    struct evbuffer *rep = bufferevent_get_input(bev);
    evbuffer_drain(rep, evbuffer_get_length(rep));

    if (conn->be_server == NULL) 
      return;

    bufferevent_setcb(conn->be_server, read_server, NULL, server_event, conn);
    bufferevent_enable(conn->be_server, EV_READ|EV_WRITE);

    (*(ctx->callback))(ctx->ctx);
    free(ctx);
  }
}

struct bufferevent * 
socks_connect(char *host, unsigned short port, void (*callback)(void *), void *ptr) 
{

  struct bufferevent *bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);

  struct socksctx *ctx = malloc(sizeof(struct socksctx));

  conn_t *conn = ptr;

  ctx->phase = 0;
  ctx->host = strdup(host);
  ctx->port = port;
  ctx->callback = callback;
  ctx->ctx = conn;

  bufferevent_setcb(bev, socks_read, NULL, socks_event, ctx);
  bufferevent_enable(bev, EV_READ|EV_WRITE);

  bufferevent_socket_connect_hostname(bev, dnsbase, AF_UNSPEC, conn->proxy->host, conn->proxy->port);

#ifdef DEBUG
    printf("CONN SOCKS ++: #%d\n", ++openconn);
#endif

  char req[3];
  req[0] = 0x05;
  req[1] = 0x01;
  req[2] = 0x00;

  bufferevent_write(bev, req, 3);
  return bev;
}
