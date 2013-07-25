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
connect_server(char *host, int port, void (*callback)(conn_t *), conn_t *conn);

void
http_direct_process(conn_t *conn);

void dis_error_msg(conn_t *conn, char *msg) {
  if (conn->be_client == NULL) return;
  error_msg(bufferevent_get_output(conn->be_client), msg);
}

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
  int read;
  struct evbuffer *output, *buffer;
  output = bufferevent_get_output(conn->be_client);
  buffer = bufferevent_get_input(conn->be_server);

  if(strcmp(conn->method, "CONNECT") == 0){
	int s;
	while((s = evbuffer_remove_buffer(buffer, output, 1024)) > 0)
	  printf("connect read size: %d\n", s);
	return;
  }

  char *line;
  char header[64], header_v[2500];
  
  if(conn->response_header == 1){
	while ((line = evbuffer_readln(buffer, NULL, EVBUFFER_EOL_CRLF)) != NULL) {
	  puts(line);
	  if (strlen(line) == 0) {
		//puts("BREAK");
		evbuffer_add_printf(output, "\r\n");
		conn->response_header = 0;
		if(conn->response_left == 0)
		  conn->no_length = 1;

		break;
	  }

      sscanf(line, "%s %s", header, header_v);
      
      if (strcmp(header, "Content-Length:") == 0)  {
		conn->response_left = atoi(header_v);
		conn->is_chunked = 0;
		printf("length found: %d\n", conn->response_left);
      }
	  else if (strcmp(header, "Connection:") == 0)  {
		if(strcmp(header_v, "close") == 0)
		  conn->connection_close = 1;
		else
		  conn->connection_close = 0;

      }
	  else if (strcmp(line, "Transfer-Encoding: chunked") == 0)  {
		conn->is_chunked = 1;
      }

	  evbuffer_add_printf(output, "%s\r\n", line);
	}
  }
  int size;
  // body
  if(conn->is_chunked == 0){

	while((read = evbuffer_remove_buffer(buffer, output, 1024)) > 0){
	  printf("not chunked read: %d\n", read);
	  conn->response_left -= read;
	}

	printf("not chunked left: %d\n", conn->response_left);

	if (conn->response_left == 0){
	  conn->response_header = 1;
	}
	return;
  }
  else {
	// chunked
	long int len;

	if(	conn->chunk_left > 0){
	  read = evbuffer_remove_buffer(buffer, output, conn->chunk_left);
	  conn->chunk_left -= read;

	  if (conn->chunk_left > 0) 
		return;
	  else if (conn->chunk_left == 0){
		evbuffer_add_printf(output, "\r\n");
		if(evbuffer_drain(buffer, 2)==-1)
		  puts("fail to drain");

	  }
	}

	while ((line = evbuffer_readln(buffer, NULL, EVBUFFER_EOL_CRLF)) != NULL) {

	  if(strlen(line)==0){
		puts("yyyyyyyyyyyyyyyyy");
		return;
	  }

	  len = strtol(line, NULL, 16);
	  printf("------:%s:-------\n", line);


	  // write chunk size
	  evbuffer_add_printf(output, "%s\r\n", line);
	  size = evbuffer_get_length(buffer);
	  conn->chunk_left = len;

	  if(len == 0){
		printf("end of response: %d\n", conn->chunk_left);
		evbuffer_add_printf(output, "\r\n");
		conn->connection_close = 1;
		conn->chunk_over = 1;
		return;
	  }

	  printf("size1: %d, left: %d\n", size, conn->chunk_left);
	  if(size == 0)
		return;

	  read = evbuffer_remove_buffer(buffer, output, len);

	  conn->chunk_left -= read;

	  printf("chunk_left: %d\n", conn->chunk_left);

	  if(conn->chunk_left == 0){

		evbuffer_add_printf(output, "\r\n");
		puts("finished");
		if(evbuffer_drain(buffer, 2)==-1)
		  puts("fail to drain");

		size = evbuffer_get_length(buffer);
		printf("size2: %d\n", size);
		if(size == 0) return;
	  }
	  else{
		puts("not finished");

		size = evbuffer_get_length(buffer);
		printf("size2: %d\n", size);
	  }
	}
  }
}

void
write_server(struct bufferevent *bev, void *ctx) {
  conn_t *conn = ctx;

  http_direct_process(conn);
}

void free_purl(conn_t *conn) {
  if (conn->purl) {
    free_parsed_url(conn->purl);

    conn->purl = NULL;
  }
}

void
free_server(conn_t *conn) {

  if (conn->be_server == NULL) return;
    
  free_purl(conn);

  bufferevent_free(conn->be_server);
  conn->be_server = NULL;

#ifdef DEBUG
  printf("CONN--: #%d\n", --openconn);
#endif

}

/* Close both client and server connection. */

void
free_conn(conn_t *conn) {

  if (conn->state) 
    free(conn->state);
  conn->state = NULL;

  if (conn->be_client)
    bufferevent_free(conn->be_client);
  conn->be_client = NULL;

  free_server(conn);
  
  free(conn);
  conn = NULL;
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

/* server event */

void 
server_event(struct bufferevent *bev, short e, void *ptr) 
{
  conn_t *conn = ptr;

  if (e & BEV_EVENT_CONNECTED) {
    if (conn->be_server)
      bufferevent_set_timeouts(conn->be_server, &cfg.timeout, &cfg.timeout);

  }
  else if (e & BEV_EVENT_EOF) {
    puts("server eof");
	conn->server_closed = 1;
  }
  else if (e & (BEV_EVENT_ERROR)) {
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

		free_conn(conn);
		return;
      }

    }
    else {
      if (code == ECONNRESET) {
		if (conn->purl == NULL || conn->purl->host == NULL) {
		  free_conn(conn);
		  return;
		}
	  
		perror("Connection reset. Added to temperary try-list"); 

		add_to_trylist(conn->purl->host);
		free_server(conn);
		error_msg(bufferevent_get_output(conn->be_client), "<!DOCTYPE HTML PUBLIC> <html> <head> <title>Connection Reset</title> </head> <body> <h1>Connection Reset</h1> Added to trylist. Proxior will use try-proxy to visit this domain next time. <hr> <address>Proxior 1.1.0</address> </body> </html> ");
		return;
      }
    }

  }
}

static void
send_req_line(conn_t *conn) {

  char *pos = conn->url + 10;
  while (pos[0] != '/')
    pos++;

  evbuffer_add_printf(bufferevent_get_output(conn->be_server), "%s %s %s\r\n", conn->method, pos, conn->version);
}

static int
prepare_connection(conn_t *conn) {

  struct state *s = conn->state; 

  struct parsed_url *url;

  if (!s->req_sent) {
    // Request line not sent.

    /* Test if connecting to a different host */

    url = simple_parse_url(conn->url);
    if (conn->be_server == NULL) {

      connect_server(url->host, url->port, http_direct_process, conn);
      
      conn->purl = url;
      return 1;
    }
    else if (conn->purl && (strcmp(conn->purl->host, url->host) || conn->purl->port != url->port)) {

      if (conn->proxy) {
		//socks5
		free_server(conn);
		init_remote_conn(conn);
		free_parsed_url(url);
		return -1;
      }

      free_server(conn);

      connect_server(url->host, url->port, http_direct_process, conn);

      free_parsed_url(conn->purl);
      conn->purl = url;

      return 1;
    }
    else {
      free_parsed_url(url);
      send_req_line(conn);

      s->req_sent = 1;
    }
  }

  return 0;
}


/* Process consistent connection. 
 * Note that in a same client connection, the request host may vary. */

void
http_direct_process(conn_t *conn) {

  if (conn->be_client == NULL) return;
  /* Keep a state */
  struct state *s = conn->state; 
  struct evbuffer *buffer;
  struct evbuffer *output;

  if (s == NULL) {
    conn->state = s = calloc(sizeof(struct state), 1);
    s->pos = STATE_HEADER;
  }

  buffer = bufferevent_get_input(conn->be_client);

  char *line;

  if (s->pos == STATE_REQ_LINE) {

    /* In the first request this won't be executed. */   
    s->pos = STATE_HEADER;

    printf("CC:");
    /* Process request line */
    if (reqest_line_process(conn)) 
      return;
  }

  if (s->pos == STATE_HEADER) {

    if (prepare_connection(conn)) 
      return;

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
      
      if (strcmp(header, "Content-Length:") == 0)  {
		s->length = atoi(header_v);
		bufferevent_setwatermark(conn->be_client, EV_READ, 0, 4096);

      }
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
   
    if (s->set_cb == 0) {

      bufferevent_setcb(conn->be_server, read_server, write_server, server_event, conn);

      bufferevent_enable(conn->be_server, EV_READ|EV_WRITE);
      s->set_cb = 1;

    }
 
    output = bufferevent_get_output(conn->be_server);

    int outsize = (int) evbuffer_get_length(output);

    if (outsize > 0) return;

    int read = evbuffer_remove_buffer(buffer, output, 1024);

    s->read += read;
    //    printf("%d %d\n", s->read, s->length);
    if (s->read != s->length) return;

    bufferevent_setcb(conn->be_server, read_server, NULL, server_event, conn);
  
  }
  else return;

 end:

  // Clean up and get ready for next request

  s->pos = STATE_REQ_LINE;
  s->req_sent = 0;
  s->read = 0;
  s->length = 0;
  s->set_cb = 0;

}

struct conn_arg {
  int port;
  void (*callback)(conn_t *);
  conn_t *conn;

  char host[512];
};

static void 
dns_cb(int result, char type, int count,
	   int ttl, void *addresses, void *arg) {

  struct conn_arg * carg = arg;

  conn_t *conn = carg->conn;

  if (result != DNS_ERR_NONE) {
    dis_error_msg(conn, "<!DOCTYPE HTML PUBLIC> <html> <head> <title>Domain Not Found</title> </head> <body> <h1>Domain Not Found</h1> Unable to resolve the server's DNS address. <br /><br /><br /> <hr /> <address>Proxior 1.1.0</address> </body> </html> ");
    return;
  }

  struct sockaddr_in sin;

  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;

  memcpy(&sin.sin_addr.s_addr, addresses, 4);

  sin.sin_port = htons(carg->port);

  conn->be_server = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
  if (bufferevent_socket_connect(conn->be_server,
								 (struct sockaddr *)&sin, sizeof(sin)) < 0) {
    /* Error starting connection */
    dis_error_msg(conn, "Cannot connect to remote server.");
    return;
  }

  bufferevent_setcb(conn->be_server, read_server, NULL, server_event, conn);

  bufferevent_enable(conn->be_server, EV_READ|EV_WRITE);

  bufferevent_setwatermark(conn->be_server, EV_READ, 0, 4096);

  hashmap_insert_ip(cfg.dnsmap, carg->host, addresses);

  carg->callback(conn);
  free(carg);
}

/* Connect server directly. */

static void
connect_server(char *host, int port, void (*callback)(conn_t *), conn_t *conn) {


  struct hashentry_s *entry = 
    hashmap_find_head(cfg.dnsmap, host);

  struct sockaddr_in sin;
  int isip = inet_pton(AF_INET, host, &(sin.sin_addr));

  if (entry == NULL && isip == 0) {

    struct conn_arg *arg = malloc(sizeof(struct conn_arg));
    arg->port = port;
    arg->callback = callback;
    arg->conn = conn;
    strcpy(arg->host, host);

    evdns_base_resolve_ipv4(dnsbase, host, DNS_QUERY_NO_SEARCH, dns_cb, arg);
    return;
  }
    

  sin.sin_family = AF_INET;
  if (isip == 0)
    memcpy(&sin.sin_addr.s_addr, entry->data, 4);

  sin.sin_port = htons(port);

  conn->be_server = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);

  bufferevent_setcb(conn->be_server, read_server, NULL, server_event, conn);

  if (bufferevent_socket_connect(conn->be_server,
								 (struct sockaddr *)&sin, sizeof(sin)) < 0) {
    /* Error starting connection */
    dis_error_msg(conn, "Cannot connect to remote server.");
    return;
  }

  bufferevent_enable(conn->be_server, EV_READ|EV_WRITE);

  callback(conn);

}

static void
read_client_socks_handshake(void *ptr) {
  conn_t *conn = ptr;

  if (conn->be_server == NULL) {

    struct parsed_url *url = simple_parse_url(conn->url);
    free_purl(conn);
    conn->purl = url;
  
    conn->be_server = socks_connect(url->host, url->port, read_client_direct, ptr);
  }
  else
    read_client_direct(ptr);

}

static int 
read_direct_https(void *ctx) {
  conn_t *conn = ctx;
  if (conn->be_server && bufferevent_read_buffer(conn->be_client, bufferevent_get_output(conn->be_server)))

	fputs("Error reading from client\n", stderr);
  return 0;
}

static void 
read_direct_https_handshake_p2(conn_t *conn) {

  if (conn->be_client == NULL) return;
  evbuffer_add_printf(bufferevent_get_output(conn->be_client), "HTTP/1.1 200 Connection established\r\n\r\n");

  conn->handshaked = 1;
}

static void 
read_direct_https_handshake(conn_t *ctx) {
  conn_t *conn = ctx;

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

  if (conn->tos == CONN_DIRECT) {
    connect_server(host, port, read_direct_https_handshake_p2, conn);
    free(url);
    return;
  }

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

  bufferevent_setcb(bevs, read_server, NULL, server_event, conn);
  bufferevent_enable(bevs, EV_READ|EV_WRITE);

  if (bufferevent_socket_connect_hostname(bevs, dnsbase, AF_UNSPEC, conn->proxy->host, conn->proxy->port) == -1) {
    perror("DNS failure\n");
    exit(1);
  }

  
#ifdef DEBUG
  printf("PROXY CONN++: #%d\n", ++openconn);
#endif

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
    return 1;
  }

  struct proxy_t *proxy = match_list(conn->url);
  if (proxy == conn->proxy && conn->be_server != NULL){
    // only socks5 or direct here
    free(line);
    return 0;
  }

  free_server(conn);
  set_conn_proxy(conn, proxy);
  init_remote_conn(conn);

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
write_client_cb(struct bufferevent *bev, void *ctx) {
  conn_t *conn = ctx;

  if(strcmp(conn->method, "CONNECT") == 0)
	return;

  // no length
  if(conn->no_length == 1 && conn->server_closed == 1){
	free_conn(conn);
	return;
  }

  // connection keep alive and finished transfer
  if(conn->connection_close == 0 && 
	 (( conn->is_chunked == 0 && conn->response_left == 0) || (conn->is_chunked == 1 && conn->chunk_over == 1 ))){
	puts("close");
	conn->chunk_over = 0;
	conn->is_chunked = 0;
	conn->response_left = 0;
	conn->lock_request = 0;
  }

  // connection close and finished transfer
  if(conn->connection_close == 1 &&
	 ((conn->is_chunked == 0 && conn->response_left == 0) || (conn->is_chunked == 1 && conn->chunk_over == 1 ))) {

	free_conn(conn);
	puts("wrote, free conn");
  }

}


static void 
client_event(struct bufferevent *bev, short e, void *ptr) {
  conn_t *conn = ptr;

  /* If client closes the connection, we can safely free server conn. */

  if (e & (BEV_EVENT_ERROR | BEV_EVENT_TIMEOUT | BEV_EVENT_EOF)){

	puts("client close");
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
  conn->response_header = 1;

  bufferevent_setcb(bev, read_client_cb, write_client_cb, client_event, conn);
  bufferevent_enable(bev, EV_READ|EV_WRITE);
}


static void
cb_timer(int sock, short what, void *arg) {
  puts("DNS Cache Purged");
  hashmap_clear(cfg.dnsmap);
}

/* start listening  */

void
start() {
  struct evconnlistener *listener;
  struct sockaddr_in sin;

  base = event_base_new();
  dnsbase = evdns_base_new(base, 1);

  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = inet_addr(cfg.listen_addr);
  sin.sin_port = htons(cfg.listen_port);

  listener = evconnlistener_new_bind(base, accept_conn_cb, NULL,
									 LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE, 128,
									 (struct sockaddr*)&sin, sizeof(sin));
  if(!listener) {
    perror("Unable to create listener");
    exit(1);
  }
  openconn = 0;

  struct timeval one_sec = { 600, 0 };
  struct event *ev;

  ev = event_new(base, -1, EV_PERSIST, cb_timer, NULL);
  event_add(ev, &one_sec);

  event_base_dispatch(base);
}
