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
  if (e & (BEV_EVENT_ERROR|BEV_EVENT_EOF|BEV_EVENT_TIMEOUT)) {
    bufferevent_free(bev);
    conn_t *conn = ((struct socksctx *)ptr)->ctx;
    conn->be_server = NULL;
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
    /* int i = 0; */
    /* for (; i< 7 + req[4]; i++) */
    /*   printf("%d\n", req[i]); */

    bufferevent_write(bev, req, 7 + req[4]);

    ctx->phase = 1;

  }
  else if (ctx->phase == 1) {

    char hs2[2];
    bufferevent_read(bev, hs2, 2);

    if (hs2[1]) {
      puts("Cannot connect to socks server");
      return;
    }

    struct evbuffer *rep = bufferevent_get_input(bev);
    evbuffer_drain(rep, evbuffer_get_length(rep));

    bufferevent_setcb(bev, read_server, NULL, server_event, ctx->ctx);

    bufferevent_enable(bev, EV_READ|EV_WRITE);

    conn_t * conn = ctx->ctx;
    struct evbuffer *output;
    if (conn->state && conn->state->header_b) {
      output = bufferevent_get_output(bev);

      evbuffer_add_printf(output, "%s %s %s\r\n", conn->method, conn->url, conn->version);

      evbuffer_remove_buffer(conn->state->header_b, output, -1);

      if (conn->state->cont_b) 
	evbuffer_remove_buffer(conn->state->cont_b, output, -1);
    }

    (*(ctx->callback))(ctx->ctx);

  }
}

struct bufferevent * 
//socks_connect(char *host, unsigned short port, void (*callback)(void (*)(void *), void *), void (*cbcb)(void *), void *conn) 
socks_connect(char *host, unsigned short port, void (*callback)(void *), void *ptr) 
{

  struct bufferevent *bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);

  struct socksctx *ctx = malloc(sizeof(struct socksctx));

  conn_t *conn = ptr;

  ctx->phase = 0;
  ctx->host = strdup(host);
  ctx->port = port;
  ctx->callback = callback;
  ctx->ctx = conn;

  bufferevent_setcb(bev, socks_read, NULL, socks_event, ctx);
  bufferevent_enable(bev, EV_READ|EV_WRITE);

  bufferevent_socket_connect_hostname(bev, NULL, AF_UNSPEC, conn->proxy->host, conn->proxy->port);

  char req[3];
  req[0] = 0x05;
  req[1] = 0x01;
  req[2] = 0x00;

  bufferevent_write(bev, req, 3);
  return bev;
}
