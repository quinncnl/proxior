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

/* Connect to server and proxy */

#include "http.h"
#include "match.h"
#include "util.h"
#include "rpc.h"
#include "common.h"
#include "socks.h"

static void
read_client_http_proxy_handshake(void *);

static int 
read_direct_http(void *);

static void
read_client_direct(void *) ;

static void
client_event(struct bufferevent *, short, void *);

static void
init_remote_conn(conn_t *);

static int
check_http_method(conn_t *conn) ;

void
error_msg(struct evbuffer *output, char *msg) {

  size_t size = strlen(msg);

  evbuffer_add_printf(output,
		      "HTTP/1.1 502 Bad Gateway\r\n"
		      "Content-Type: text/html\r\n"
		      "Access-Control-Allow-Origin: *\r\n"
		      "Connection: close\r\n"
		      "Cache-Control: no-cache\r\n"
		      "Content-Length: %d\r\n\r\n", (int) size);
  
  evbuffer_add(output, msg, size);
}

/* read data from server to client */

void
read_server(struct bufferevent *bev, void *ctx) {
  conn_t *conn = ctx;
  char version[10];
  int code;

  if (conn->not_headline == 0) {
    char buf[64];

    evbuffer_copyout(bufferevent_get_input(bev), buf, 64);

    if (strlen(buf) == 0) return;

    sscanf(buf, "%s %d", version, &code);
    
    // Caused by reset or timeout from proxy server.
    if (code == 502)
      log_error(502, "Bad gateway.", conn->url, conn->proxy);

    conn->not_headline = 1;
  }

  if (bufferevent_read_buffer(bev, bufferevent_get_output(conn->be_client))) 
    fputs("Error reading from server.", stderr);

}


void
free_server(conn_t *conn) {
  if (conn->be_server) 
    bufferevent_free(conn->be_server);

  else return;

  conn->be_server = NULL;
}

/* Close both client and server connection. */

void
free_conn(conn_t *conn) {

  if (conn->state) {
    if (conn->state->cont) 
      evbuffer_free(conn->state->cont);
    
    if (conn->state->header) {
      evbuffer_free(conn->state->header);  
      evbuffer_free(conn->state->header_b);

      if (conn->state->cont_b) 
	evbuffer_free(conn->state->cont_b);   
    }

    free(conn->state);
  }

  bufferevent_free(conn->be_client);

  free_server(conn);
  
  if (conn->purl) free_parsed_url(conn->purl); 
  free(conn);
}

static void
set_conn_proxy(conn_t *conn, struct proxy_t *proxy) {

  conn->proxy = proxy;
  if (proxy == NULL) {
    conn->tos = DIRECT;
    return ;
  }

  switch (proxy->type) {
  case HTTP:
    conn->tos = HTTP_PROXY;
    break;
  case SOCKS:
    conn->tos = SOCKS_PROXY;
  }
}

void server_connected(conn_t *conn) 
{

  bufferevent_set_timeouts(conn->be_server, &config->timeout, &config->timeout);

  bufferevent_setcb(conn->be_server, read_server, NULL, server_event, conn);

  bufferevent_enable(conn->be_server, EV_READ|EV_WRITE);
}

/* server event */

void 
server_event(struct bufferevent *bev, short e, void *ptr) 
{
  conn_t *conn = ptr;

  if (e & BEV_EVENT_CONNECTED) 
    server_connected (conn);

  else if (e & BEV_EVENT_EOF) {

    free_server(conn);

  }
  else if (e & (BEV_EVENT_ERROR|BEV_EVENT_TIMEOUT)) {

    if (e & BEV_EVENT_ERROR) {

      int code = evutil_socket_geterror(bufferevent_getfd(conn->be_server)); 

      if (code != EAGAIN && code != EPIPE) 

	log_error(code,  evutil_socket_error_to_string(code), conn->url, conn->proxy);

     
      if (conn->proxy == config->try_proxy) {
	if (config->try_proxy != NULL) {
	  char msg1[50] =  "Error connecting to proxy ";
	  strcat(msg1, conn->proxy->name);
	  puts(msg1);
	  error_msg(bufferevent_get_output(conn->be_client), msg1);
	  return;

	}
	else {

	  error_msg(bufferevent_get_output(conn->be_client), "No try-proxy set. Direct connection failed. Check log.");
	  return;
	}

      }
      else {
	perror("Using try-proxy");	
	free_server(conn);

	set_conn_proxy(conn, config->try_proxy);
	init_remote_conn(conn);

	return;
      }
    }//error

    free_server(conn);

  }
}


void
http_ready_cb(int (*callback)(void *ctx), void *ctx) {
  conn_t *conn = ctx;
  struct state *s = conn->state; // s for shorthand
  struct evbuffer *buffer = bufferevent_get_input(conn->be_client);
  
  if (s == NULL) {
    conn->state = s = calloc(sizeof(struct state), 1);
    s->req_rcvd = 1;
    s->header = evbuffer_new();
    s->header_b = evbuffer_new();
    s->cont_b = evbuffer_new();
  }

  // header section
  if (!s->is_cont) {
    char *line;
    char header[64], header_v[2500];
    
    if (!s->req_rcvd) {
      line = evbuffer_readln(buffer, NULL, EVBUFFER_EOL_CRLF);

      if (line == NULL) return;

      if (strlen(line) > MAX_URL_LEN) {
	free(line);
	free_server(conn);
	fprintf(stderr, "URL TOO LONG.");
	return;
      }

      sscanf(line, "%s %s %s", conn->method, conn->url, conn->version);

      if (check_http_method(conn)) return; 

      printf("CC: %s %s\n", conn->method, conn->url);
      free(line);

      /* First line in request has not been sent here. */

      s->req_rcvd = 1;

      set_conn_proxy(conn, match_list(conn->url));
      init_remote_conn(conn);
      return;
    }

    /* Forward and find content length */

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

  /* If callback function succeeded(header sent), switch header and header_b. */

  if ((*callback)(ctx)) return;

  // Clean up and get ready for next request

  evbuffer_drain(s->header_b,evbuffer_get_length(s->header_b));

  struct evbuffer *tmp = s->header_b;
  s->header_b = s->header;
  s->header = tmp;

  if (s->length) {
    evbuffer_drain(s->cont_b,evbuffer_get_length(s->cont_b));
    tmp = s->cont_b;
    s->cont_b = s->cont;
    s->cont = tmp;
  }

  s->req_rcvd = 0;
  s->read = 0;
  s->length = 0;
  s->is_cont = 0;

}

static void
connect_server(char *host, int port, conn_t *conn) {

  conn->be_server = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);

  bufferevent_setcb(conn->be_server, read_server, NULL, server_event, conn);
  bufferevent_enable(conn->be_server, EV_READ|EV_WRITE);

  if(bufferevent_socket_connect_hostname(conn->be_server, NULL, AF_UNSPEC, host, port) == -1) {
    perror("DNS failure\n");
    exit(1);
  }
}

static void
read_client_socks_handshake(void *ptr) {
  conn_t *conn = ptr;

  if (conn->be_server == NULL) {

    struct parsed_url *url = simple_parse_url(conn->url);
    conn->purl = url;
  
    conn->be_server = socks_connect(url->host, url->port, read_client_direct, ptr);
  }
  else
    read_client_direct(ptr);

}

/* Return 1 if data not sent */

static int 
read_direct_http(void *ctx) {
  conn_t *conn = ctx;

  // e.g. GET http://www.wangafu.net/~nickm/libevent-book/Ref6_bufferevent.html HTTP/1.1

  struct parsed_url *url;
  url = simple_parse_url(conn->url);

  if (conn->tos == DIRECT) {
    
    if (conn->be_server == NULL) {
      connect_server(url->host, url->port, conn);

    }
    else if (strcmp(conn->purl->host, url->host) || conn->purl->port != url->port) {

      free_parsed_url(conn->purl);   

      bufferevent_free(conn->be_server);
      conn->be_server = NULL;

      connect_server(url->host, url->port, conn);

    }
    conn->purl = url;

  }
  else if (strcmp(conn->purl->host, url->host) 
	   || conn->purl->port != url->port) {

    free_parsed_url(conn->purl);  
 
    free_parsed_url(url);

    bufferevent_free(conn->be_server);
    conn->be_server = NULL;

    read_client_socks_handshake(conn);
    
    return 1;
  }

  struct evbuffer *output = bufferevent_get_output(conn->be_server);

  char *pos = conn->url + 10;
  while (pos[0] != '/')
    pos++;

  evbuffer_add_printf(output, "%s %s %s\r\n", conn->method, pos, conn->version);

  /* Need to keep a copy of HTTP request in case connection be reset. Evbuffer cannot be drained!!! */

  int size = evbuffer_get_length(conn->state->header);
  void *data = evbuffer_pullup(conn->state->header, size);

  bufferevent_write(conn->be_server, data, size);

  if (conn->state->length) {

    size = evbuffer_get_length(conn->state->cont);
    data = (void *) evbuffer_pullup(conn->state->cont, size);
    bufferevent_write(conn->be_server, data, size);
  }

  return 0;
}

static int 
read_direct_https(void *ctx) {
  conn_t *conn = ctx;
  if (conn->be_server && bufferevent_read_buffer(conn->be_client, bufferevent_get_output(conn->be_server)))

  fputs("Error reading from client\n", stderr);
  return 0;
}

static int 
read_direct_https_handshake(void *ctx) {
  conn_t *conn = ctx;

  // e.g. CONNECT www.wikipedia.org:443 HTTP/1.1

  conn->state = calloc(sizeof(struct state), 1);

  char *url = strdup(conn->url);
  char *host = strtok(url, ":");
  int port = atoi(strtok(NULL, ""));

  char *tmp;

  // We don't really care what headers there are

  while ((tmp = evbuffer_readln(bufferevent_get_input(conn->be_client), NULL, EVBUFFER_EOL_CRLF )) != NULL) {
    if (strlen(tmp) == 0) {
      free(tmp);
      break;
    }
    free(tmp);
  }

  if (conn->tos == DIRECT)
    connect_server(host, port, conn);
  
  free(url);
  evbuffer_add_printf(bufferevent_get_output(conn->be_client), "HTTP/1.1 200 Connection established\r\n\r\n");

  conn->handshaked = 1;
  return 1;
}

/* Connect to server directly. Need to take care of both http and https */

static void
read_client_direct(void *ctx) {
  conn_t *conn = ctx;

  if (strcmp(conn->method, "CONNECT") == 0) {

    if (conn->handshaked)
      read_direct_https(ctx);
    else
      http_ready_cb(read_direct_https_handshake, ctx);
  }
  else {
    http_ready_cb(read_direct_http, ctx);
  }
}

/* Connect to HTTP proxy server */

static void
read_client_http_proxy_handshake(void *ptr) {
  conn_t *conn = ptr;
  struct bufferevent *bevs;

  bevs = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);

  conn->be_server = bevs;

  struct evbuffer *output = bufferevent_get_output(bevs);

  evbuffer_add_printf(output, "%s %s %s\r\n", conn->method, conn->url, conn->version);

  if (conn->state && conn->state->header_b) {
    evbuffer_remove_buffer(conn->state->header_b, output, -1);
    if (conn->state->cont_b) 
      evbuffer_remove_buffer(conn->state->cont_b, output, -1);
  }
  else 
    bufferevent_read_buffer(conn->be_client, output);

  if (conn->state && conn->state->cont) 
    evbuffer_remove_buffer(conn->state->cont, output, -1);

  bufferevent_setcb(bevs, NULL, NULL, server_event, conn);
  bufferevent_enable(bevs, EV_READ|EV_WRITE);

  if (bufferevent_socket_connect_hostname(bevs, NULL, AF_UNSPEC, conn->proxy->host, conn->proxy->port) == -1) {
    perror("DNS failure\n");
    exit(1);
  }
}

static void
read_client_http_proxy(struct bufferevent *bev, void *ctx) 
{
  conn_t *conn = ctx;
  if (conn->be_server && bufferevent_read_buffer(bev, bufferevent_get_output(conn->be_server)))
    printf("error reading from client\n");

}

static void
init_remote_conn(conn_t *conn) {

  if (conn->proxy != NULL) {
    if (conn->proxy->type == HTTP) {
      read_client_http_proxy_handshake(conn);

      bufferevent_setcb(conn->be_client, read_client_http_proxy, NULL, client_event, conn);

    }
    else if (conn->proxy->type == SOCKS) {

      read_client_socks_handshake(conn);
    }
  }
  else 
    read_client_direct(conn);

}

static int
check_http_method(conn_t *conn) {
  static const char *valid_method[] = {
    "GET", 
    "CONNECT",
    "POST",
    "PUT",
    "DELETE",    
    "OPTIONS"
  };
  int i;
  for (i = 0; i < 6; i++) {
    if (strcmp(conn->method, valid_method[i]) == 0) 
      return 0;
  }
  
  error_msg(bufferevent_get_output(conn->be_client), "BAD REQUEST");
  free_server(conn);
  return -1;
}

static void
read_client(struct bufferevent *bev, void *ctx) {
  conn_t *conn = ctx;

  if (conn->tos == DIRECT || conn->tos == SOCKS_PROXY) {
    read_client_direct(conn);
    return;
  }

  /* The first time talking to a client.
     Find out requested url and apply switching rules  */

  char *line;
  line = evbuffer_readln(bufferevent_get_input(bev), NULL, EVBUFFER_EOL_CRLF);

  if (line == NULL) return;

  if (strlen(line) > MAX_URL_LEN) {
    free(line);
    free_server(conn);
    fprintf(stderr, "URL TOO LONG.");
    return;
  }

  sscanf(line, "%s %s %s", conn->method, conn->url, conn->version);

  if (check_http_method(conn)) return; 

  printf("%s %s\n", conn->method, conn->url);

  free(line);

  if (conn->url[0] == '/' ){
    rpc(bev, ctx);
    conn->tos = CONFIG;

    bufferevent_setcb(bev, rpc, NULL, client_event, conn);
    return;
  }

  set_conn_proxy(conn, match_list(conn->url));
  init_remote_conn(conn);

}

static void 
client_event(struct bufferevent *bev, short e, void *ptr) {
  conn_t *conn = ptr;

  if (e & (BEV_EVENT_ERROR|BEV_EVENT_EOF)){
      free_conn(conn);
    }
}

static void
accept_conn_cb(struct evconnlistener *listener,
	       evutil_socket_t fd, struct sockaddr *address, int socklen,
	       void *ctx) {
  struct bufferevent *bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);

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
