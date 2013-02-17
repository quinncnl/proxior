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

/* Connect to server and proxy */

#include "http.h"
#include "match.h"
#include "util.h"
#include "rpc.h"
#include <errno.h>
#include <event2/util.h>
#include <event2/event.h>
#include <stdarg.h>
#include <arpa/inet.h>


static void
read_client_http_proxy(struct bufferevent *bev, void *ptr);

/* read data from server to client */

static void
read_server(struct bufferevent *bev, void *ctx) {
  conn_t *conn = ctx;
  struct bufferevent *bev_client = conn->be_client;
  char version[10];
  int code;
  
  if (conn->headline) {
    char buf[64];
    evbuffer_copyout(bufferevent_get_input(bev), buf, 64);
    sscanf(buf, "%s %d", version, &code);
    if (code == 502)
      log_error(502, "Bad gateway.", conn->url, conn->proxy);

    conn->headline = 0;
  }

  if (bufferevent_read_buffer(bev, bufferevent_get_output(bev_client))) 
    fputs("Error reading from server.", stderr);

}


static void
free_conn(conn_t *conn) {

  if (conn->state) {
    if (conn->state->cont) 
      evbuffer_free(conn->state->cont);
    
    if (conn->state->header) 
      evbuffer_free(conn->state->header);  

    free(conn->state);
  }

  bufferevent_free(conn->be_client);
  if (conn->be_server) 
    bufferevent_free(conn->be_server);
  
  free(conn);
}

/* server event */

static void 
server_event(struct bufferevent *bev, short e, void *ptr) {
  conn_t *conn = ptr;
  // perror("SERVER EVENT");
  if (e & BEV_EVENT_CONNECTED) {
    
    conn->headline = 1;
    bufferevent_set_timeouts(bev, &config->timeout, &config->timeout);
    bufferevent_setcb(bev, read_server, NULL, server_event, conn);
    bufferevent_enable(bev, EV_READ|EV_WRITE);
    
  }
  else if (e & (BEV_EVENT_ERROR|BEV_EVENT_EOF|BEV_EVENT_TIMEOUT)) {

    if (e & BEV_EVENT_ERROR) {

      int code = evutil_socket_geterror(bufferevent_getfd(conn->be_server)); 

      log_error(code,  evutil_socket_error_to_string(code), conn->url, conn->proxy);

      perror("Using try-proxy.");
      conn->proxy = config->try_proxy;
      conn->tos = HTTP_PROXY;

      bufferevent_free(conn->be_server);

      read_client_http_proxy(conn->be_client, conn);
      return;
    }

    free_conn(conn);

  }
}

void
http_ready_cb(void (*callback)(void *ctx), void *ctx) {
  conn_t *conn = ctx;
  struct state *s = conn->state; // s for shorthand
  struct evbuffer *buffer = bufferevent_get_input(conn->be_client);
  
  if (s == NULL) {
    conn->state = s = calloc(sizeof(struct state), 1);
    s->eor = 0;
    s->header = evbuffer_new();
  }

  // header section
  if (!s->is_cont) {
    char *line;
    char header[64], header_v[2500];
    
    if (s->eor) {
      line = evbuffer_readln(buffer, NULL, EVBUFFER_EOL_CRLF);

      sscanf(line, "%s %s %s", conn->method, conn->url, conn->version);

      // Don't store request line(first line) in header.
      // evbuffer_add_printf(s->header, "%s\r\n", line);

      printf("CC: %s %s\n", conn->method, conn->url);
      free(line);
      s->eor = 0;
    }

    // find content length
    while ((line = evbuffer_readln(buffer, NULL, EVBUFFER_EOL_CRLF)) != NULL) {

      if (strlen(line) == 0) {   
	evbuffer_add_printf(s->header, "\r\n");   
	free(line);
	s->is_cont = 1;
	break;
      }

 
      sscanf(line, "%s %s", header, header_v);

      
      if (strcmp(header, "Content-Length:") == 0) 
	s->length = atoi(header_v);

      if (strcmp(header, "Proxy-Connection:")) 
	evbuffer_add_printf(s->header, "%s\r\n", line);

      free(line);
    } // while
  } // header

  // content section
  if (s->is_cont) {
    // simply a header, no content
    if (s->length == 0) goto end;  

    if (s->cont == NULL) s->cont = evbuffer_new();

    s->read += evbuffer_remove_buffer(buffer, s->cont, s->length);

    if (s->read != s->length) return;
  }
  else return;

  end:
  (*callback)(ctx);

  // clean up and get ready for next request
  s->eor = 1;
  s->read = 0;
  s->length = 0;
  s->is_cont = 0;
}

static void
connect_server(char *host, int port, conn_t *conn) {

  conn->be_server = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);

  bufferevent_setcb(conn->be_server, read_server, NULL, server_event, conn);
  bufferevent_enable(conn->be_server, EV_READ|EV_WRITE);

  if(bufferevent_socket_connect_hostname(conn->be_server, NULL, AF_UNSPEC, host, port) == -1) {
    perror("DNS failure\n");
    exit(1);
  }
}

static void 
read_direct_http(void *ctx) {
  conn_t *conn = ctx;

  // e.g. GET http://www.wangafu.net/~nickm/libevent-book/Ref6_bufferevent.html HTTP/1.1
  if (conn->be_server == NULL) {
    struct parsed_url *url = simple_parse_url(conn->url);

    connect_server(url->host, url->port, conn);
    free_parsed_url(url);
  }

  struct evbuffer *output = bufferevent_get_output(conn->be_server);

  char *pos = conn->url + 10;
  while (pos[0] != '/')
    pos++;

  evbuffer_add_printf(output, "%s %s %s\r\n", conn->method, pos, conn->version);

  int header_s = evbuffer_get_length(conn->state->header);

  char *header = malloc(header_s);

  evbuffer_copyout(conn->state->header, header, header_s);

  //printf("%s", header);

  evbuffer_add(output, header, header_s);
  free(header);

  if (conn->state->length) {
    char *cont = malloc(conn->state->length);
  
    evbuffer_copyout(conn->state->cont, cont, conn->state->length);

    evbuffer_add(output, cont, conn->state->length);
    free(cont); 
  }
}

static void 
read_direct_https(void *ctx) {
  conn_t *conn = ctx;
  if (bufferevent_read_buffer(conn->be_client, bufferevent_get_output(conn->be_server)))

  fputs("error reading from client", stderr);
}

static void 
read_direct_https_handshake(void *ctx) {
  conn_t *conn = ctx;

  // e.g. CONNECT www.wikipedia.org:443 HTTP/1.1

  conn->state = calloc(sizeof(struct state), 1);

  char *url = strdup(conn->url);
  char *host = strtok(url, ":");
  int port = atoi(strtok(NULL, ""));

  char *tmp;
  // we don't really care what headers there are, just find the end of it

  while ((tmp = evbuffer_readln(bufferevent_get_input(conn->be_client), NULL, EVBUFFER_EOL_CRLF )) != NULL) {
    if (strlen(tmp) == 0) {
      free(tmp);
      break;
    }
    free(tmp);
  }

  connect_server(host, port, conn);
  
  free(url);
  evbuffer_add_printf(bufferevent_get_output(conn->be_client), "HTTP/1.1 200 Connection established\r\n\r\n");

}

/* Connect to server directly. Need to take care of both http and https */

static void
read_client_direct(struct bufferevent *bev, void *ctx) {
  conn_t *conn = ctx;

  if (strcmp(conn->method, "CONNECT") == 0) {
    if (conn->be_server)
      read_direct_https(ctx);
    else
      http_ready_cb(read_direct_https_handshake, ctx);
  }
  else {
    http_ready_cb(read_direct_http, ctx);
  }
}

static void
read_client_http_proxy(struct bufferevent *bev, void *ptr) {
  conn_t *conn = ptr;
  struct bufferevent *bevs;

  bevs = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);

  conn->be_client = bev;
  conn->be_server = bevs;

  struct evbuffer *output = bufferevent_get_output(bevs);

  evbuffer_add_printf(output, "%s %s %s\r\n", conn->method, conn->url, conn->version);

  if (conn->state && conn->state->header) {
    evbuffer_remove_buffer(conn->state->header, output, -1);
  }
  else 
    bufferevent_read_buffer(bev, output);

  bufferevent_setcb(bevs, NULL, NULL, server_event, conn);
  bufferevent_enable(bevs, EV_READ|EV_WRITE);

  if(bufferevent_socket_connect_hostname(bevs, NULL, AF_UNSPEC, conn->proxy->host, conn->proxy->port) == -1) {
    perror("DNS failure\n");
    exit(1);
  }
}


static void
read_client(struct bufferevent *bev, void *ctx) {
  conn_t *conn = ctx;

  if (conn->tos == DIRECT) {
    read_client_direct(bev, conn);
    return;
  }

  if (conn->tos == HTTP_PROXY) {

    if (bufferevent_read_buffer(bev, bufferevent_get_output(conn->be_server)))
      printf("error reading from client\n");

    return;
  }

  if (conn->tos == CONFIG) {
    rpc(ctx);
    return;
  }

  /* the first time talk to client
find out requested url and apply switching rules  */

  char *line;
  line = evbuffer_readln(bufferevent_get_input(bev), NULL, EVBUFFER_EOL_ANY);

  sscanf(line, "%s %s %s", conn->method, conn->url, conn->version);

  printf("%s %s\n", conn->method, conn->url);

  free(line);

  if (conn->url[0] == '/' ){
    rpc(ctx);
    conn->tos = CONFIG;
    
    return;
  }

/* Determine the connection method by the first header of a consistent connection. Note that there is a slight chance of mis-choosing here. We just ignore it here for efficiency. */

  conn->proxy = match_list(conn->url);

  if (conn->proxy != NULL) {
    conn->tos = HTTP_PROXY;
    read_client_http_proxy(bev, conn);
  }
  else {
    conn->tos = DIRECT;
    read_client_direct(bev, conn);
  }
}


static void 
client_event(struct bufferevent *bev, short e, void *ptr) {
  conn_t *conn = ptr;

  if (e & BEV_EVENT_CONNECTED) 
    bufferevent_set_timeouts(bev, &config->timeout, &config->timeout);

  if (e & (BEV_EVENT_ERROR|BEV_EVENT_EOF|BEV_EVENT_TIMEOUT)){

      // close connection
      free_conn(conn);
    }
}

static void
accept_conn_cb(struct evconnlistener *listener,
	       evutil_socket_t fd, struct sockaddr *address, int socklen,
	       void *ctx) {
  struct bufferevent *bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);

  conn_t *conn = calloc(sizeof(conn_t), 1);

  conn->be_client = bev;

  bufferevent_setcb(bev, read_client, NULL, client_event, conn);
  bufferevent_enable(bev, EV_READ|EV_WRITE);
}


/* start listening  */

void
start() {
  struct evconnlistener *listener;
  struct sockaddr_in sin;

  base = event_base_new();

  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = inet_addr(config->listen_addr);
  sin.sin_port = htons(config->listen_port);

  listener = evconnlistener_new_bind(base, accept_conn_cb, NULL,
				     LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE, -1,
				     (struct sockaddr*)&sin, sizeof(sin));
  if(!listener) {
    perror("Unable to create listener");
    exit(1);
  }

  event_base_dispatch(base);
}
