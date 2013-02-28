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

static void
read_client_direct(void *) ;

static void
client_event(struct bufferevent *, short, void *);

static void
init_remote_conn(conn_t *);

static int
validate_http_method(conn_t *conn) ;

static int
reqest_line_process(conn_t *conn);

static void
connect_server(char *host, int port, conn_t *conn);

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
    conn->tos = CONN_DIRECT;
    return ;
  }

  switch (proxy->type) {
  case PROXY_HTTP:
    conn->tos = CONN_HTTP_PROXY;
    break;
  case PROXY_SOCKS:
    conn->tos = CONN_SOCKS_PROXY;
  }
}

void server_connected(conn_t *conn) 
{

  bufferevent_set_timeouts(conn->be_server, &cfg.timeout, &cfg.timeout);

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

     
      if (conn->proxy == cfg.try_proxy) {
	if (cfg.try_proxy != NULL) {
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
	perror("Connection failed. Added to temperary try-list"); 

	free_server(conn);

	add_to_trylist(conn->purl->host);

	error_msg(bufferevent_get_output(conn->be_client), "Connection reset. Using try-proxy next time.");
	return;
      }
    }//error

    free_server(conn);

  }
}

static void
send_req_line(conn_t *conn) {

  char *pos = conn->url + 10;
  while (pos[0] != '/')
    pos++;

  evbuffer_add_printf(bufferevent_get_output(conn->be_server), "%s %s %s\r\n", conn->method, pos, conn->version);

}

static void
prepare_connection(conn_t *conn) {

  struct state *s = conn->state; 

  if (!s->req_sent) {

    struct parsed_url *url;
    url = simple_parse_url(conn->url);

    if (conn->be_server == NULL) 
      connect_server(url->host, url->port, conn);

    else if (strcmp(conn->purl->host, url->host) || conn->purl->port != url->port) {

      free_parsed_url(conn->purl);   

      free_server(conn);

      connect_server(url->host, url->port, conn);

    }

    conn->purl =  url;
    send_req_line(conn);

    s->req_sent = 1;
  }
}


/* Process consistent connection. 
 * Note that in a same client connection, the request host may vary. */

void
http_direct_process(conn_t *conn) {

  /* Keep a state */
  struct state *s = conn->state; 
  struct evbuffer *buffer = bufferevent_get_input(conn->be_client);
  struct evbuffer *output;
  
  if (s == NULL) {
    conn->state = s = calloc(sizeof(struct state), 1);
    s->pos = STATE_HEADER;
  }

  char *line;
  if (s->pos == STATE_REQ_LINE) {
    s->pos = STATE_HEADER;

    /* Process request line */
    reqest_line_process(conn);
    return;
  }

  prepare_connection(conn);
  if (s->pos == STATE_HEADER) {

    char header[64], header_v[2500];
    
    output = bufferevent_get_output(conn->be_server);

    /* Forward and find content length */
    while ((line = evbuffer_readln(buffer, NULL, EVBUFFER_EOL_CRLF)) != NULL) {

      if (strlen(line) == 0) {
	evbuffer_add_printf(output, "\r\n");

	free(line);
	s->pos = STATE_BODY;
	goto read_body;
      }
 
      sscanf(line, "%s %s", header, header_v);
      
      if (strcmp(header, "Content-Length:") == 0) 
	s->length = atoi(header_v);

      /* Skip proxy-connection */

      if (strcmp(header, "Proxy-Connection:")) {
	evbuffer_add_printf(output, "%s\r\n", line);

      }
      free(line);
    } // while

    return;
  } // header

 read_body:

  if (s->pos == STATE_BODY) {

    if (s->length == 0) goto end; 

    output = bufferevent_get_output(conn->be_server);

    s->read += evbuffer_remove_buffer(buffer, output, evbuffer_get_length(buffer));

    if (s->read != s->length) return;
  }
  else return;

  end:

  // Clean up and get ready for next request

  s->pos = STATE_REQ_LINE;
  s->req_sent = 0;
  s->read = 0;
  s->length = 0;

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
/*
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
*/
static int 
read_direct_https(void *ctx) {
  conn_t *conn = ctx;
  if (conn->be_server && bufferevent_read_buffer(conn->be_client, bufferevent_get_output(conn->be_server)))

  fputs("Error reading from client\n", stderr);
  return 0;
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

  // We don't really care what headers there are

  while ((tmp = evbuffer_readln(bufferevent_get_input(conn->be_client), NULL, EVBUFFER_EOL_CRLF )) != NULL) {
    if (strlen(tmp) == 0) {
      free(tmp);
      break;
    }
    free(tmp);
  }

  if (conn->tos == CONN_DIRECT)
    connect_server(host, port, conn);
  
  free(url);
  evbuffer_add_printf(bufferevent_get_output(conn->be_client), "HTTP/1.1 200 Connection established\r\n\r\n");

  conn->handshaked = 1;

}

/* Connect to server directly. 
 * Need to take care of both http and https 
*/

static void
read_client_direct(void *ctx) {
  conn_t *conn = ctx;

  if (strcmp(conn->method, "CONNECT") == 0) {

    if (conn->handshaked)
      read_direct_https(ctx);
    else
      read_direct_https_handshake(ctx);
  }
  else {
    http_direct_process(conn);
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

  bufferevent_read_buffer(conn->be_client, output);

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

/* Start a connection to server or proxy. The request line has been read. */

static void
init_remote_conn(conn_t *conn) {

  if (conn->proxy != NULL) {
    if (conn->proxy->type == PROXY_HTTP) {
      read_client_http_proxy_handshake(conn);

      bufferevent_setcb(conn->be_client, read_client_http_proxy, NULL, client_event, conn);

    }
    else if (conn->proxy->type == PROXY_SOCKS) {

      read_client_socks_handshake(conn);
    }
  }
  else 
    read_client_direct(conn);

}

static int
validate_http_method(conn_t *conn) {
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

static int
validate_req_line(conn_t *conn, char *line) 
{
  if (strlen(line) > MAX_URL_LEN) {

    free_server(conn);
    fprintf(stderr, "URL TOO LONG.");
    return -1;
  }
  return 0;
}

/* Read request line.
 * Return 1 if request has not been read.
 * Return -1 if validation failed. */

static int
reqest_line_process(conn_t *conn) {
  char *line;

  line = evbuffer_readln(bufferevent_get_input(conn->be_client), NULL, EVBUFFER_EOL_CRLF);

  if (line == NULL) return 1;

  if (validate_req_line(conn, line)) goto fail;

  sscanf(line, "%s %s %s", conn->method, conn->url, conn->version);

  if (validate_http_method(conn)) goto fail;

  printf("%s %s\n", conn->method, conn->url);

  if (conn->url[0] == '/' ){
    rpc(conn->be_client, conn);
    conn->tos = CONN_CONFIG;

    bufferevent_setcb(conn->be_client, rpc, NULL, client_event, conn);

    free(line);
    return 0;
  }

  set_conn_proxy(conn, match_list(conn->url));
  init_remote_conn(conn);
  free(line);
  return 0;

 fail:
  free(line);
  return -1;
}

static void
read_client_cb(struct bufferevent *bev, void *ctx) {
  conn_t *conn = ctx;

  if (conn->tos == CONN_DIRECT || conn->tos == CONN_SOCKS_PROXY) {
    read_client_direct(conn);
    return;
  }

  /* The first time talking to a client.
     Find out requested url and apply switching rules  */

  reqest_line_process(conn);
}

static void 
client_event(struct bufferevent *bev, short e, void *ptr) {
  conn_t *conn = ptr;

  /* If client closes the connection, we can safely free server conn. */

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

  bufferevent_setcb(bev, read_client_cb, NULL, client_event, conn);
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
  sin.sin_addr.s_addr = inet_addr(cfg.listen_addr);
  sin.sin_port = htons(cfg.listen_port);

  listener = evconnlistener_new_bind(base, accept_conn_cb, NULL,
				     LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE, -1,
				     (struct sockaddr*)&sin, sizeof(sin));
  if(!listener) {
    perror("Unable to create listener");
    exit(1);
  }

  event_base_dispatch(base);
}
