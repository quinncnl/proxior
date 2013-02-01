#include "http.h"
#include "match.h"
#include "util.h"
#include "rpc.h"
#include <errno.h>
#include <event2/util.h>
#include <stdarg.h>

/* Connect to server and proxy */

typedef struct conn
{
  struct bufferevent *be_client, *be_server;
  char url[MAX_URL_LEN];
  char method[10];
  struct proxy_t *proxy;
  int pos;
} conn_t;

/* read data from server to client */

static void
read_server(struct bufferevent *bev, void *ctx) {
  conn_t *conn = ctx;
  struct bufferevent *bev_client = conn->be_client;
  char version[10];
  int code;

#ifdef DEBUG

  char *buf = malloc(1024);
  evbuffer_copyout(bufferevent_get_input(bev), buf, 1024);
  printf("SERVER HEADER: %s\n", buf);
#endif

  if (conn->pos == 0) {
    char *buf = malloc(128);
    evbuffer_copyout(bufferevent_get_input(bev), buf, 128);
    sscanf(buf, "%s %d", version, &code);
    if (code == 502) {
      log_reset(conn->url);
    }
    free(buf);
    conn->pos = 1;
  }

  bufferevent_read_buffer(bev, bufferevent_get_output(bev_client));

}

/* server event */

static void 
server_event(struct bufferevent *bev, short e, void *ptr) {
  conn_t *conn = ptr;
  if (e & BEV_EVENT_CONNECTED) {
    //
    conn->pos = 0;
    bufferevent_set_timeouts(bev, &config->timeout, &config->timeout);
    bufferevent_setcb(bev, read_server, NULL, server_event, conn);
    if (bufferevent_enable(bev, EV_READ|EV_WRITE) == -1) perror("S");
    
  }
  else if (e & (BEV_EVENT_ERROR|BEV_EVENT_EOF|BEV_EVENT_TIMEOUT)) {

    if (e & BEV_EVENT_ERROR) {

      int err = evutil_socket_geterror(bufferevent_getfd(conn->be_server)); 
      //printf("error code: %d, string: %s\n", err, evutil_socket_error_to_string(err)); 
      log_reset(conn->url);

    }
    
    bufferevent_free(conn->be_client);
    bufferevent_free(bev);
    free(conn);

  }
}

/* connect to server directly. need to take care of both http and htts */

static void
read_client_direct(struct bufferevent *bev, void *ctx) {
  conn_t *conn = ctx;
  struct bufferevent *bevs;
  struct parsed_url *purl = NULL;

  char *host, *port, *url = NULL; 
  int i_port;

  bevs = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);
  conn->be_client = bev;
  conn->be_server = bevs;

  if (strcmp(conn->method, "CONNECT") == 0) 
    {
      // e.g. CONNECT www.wikipedia.org:443 HTTP/1.1
      url = malloc(strlen(conn->url)+1);
      strcpy(url, conn->url);
      host = strtok(url, ":");
      port = strtok(NULL, "");
      i_port = atoi(port);

      
      char *tmp;
      while ((tmp = evbuffer_readln(bufferevent_get_input(bev), NULL, EVBUFFER_EOL_ANY ))!=NULL);
      //	printf("%s\n", tmp);
      

      evbuffer_add_printf(bufferevent_get_output(bev), "HTTP/1.1 200 Connection established\r\n\r\n");
    }
  // end of https
  else 
    {
      // e.g. GET http://www.wangafu.net/~nickm/libevent-book/Ref6_bufferevent.html HTTP/1.1
      purl = simple_parse_url(conn->url);
      i_port = purl->port;
      host = purl->host;

      bufferevent_read_buffer(bev, bufferevent_get_output(bevs));

    }

  bufferevent_setcb(bevs, read_server, NULL, server_event, conn);
  bufferevent_enable(bevs, EV_READ|EV_WRITE);

  if(bufferevent_socket_connect_hostname(bevs, NULL, AF_UNSPEC, host, i_port) == -1) {
    perror("DNS failure\n");
    exit(1);
  }
  if (purl != NULL) {
    free(purl->url);
    free(purl->host);
    free(purl);
  }
  if (url != NULL) free(url);
}

static void
read_client_http_proxy(struct bufferevent *bev, void *ptr) {
  conn_t *conn = ptr;
  struct bufferevent *bevs;

  bevs = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);

  conn->be_client = bev;
  conn->be_server = bevs;

  if (bufferevent_read_buffer(bev, bufferevent_get_output(bevs)) == -1) 
    perror("error");

  bufferevent_setcb(bevs, NULL, NULL, server_event, conn);

  if(bufferevent_socket_connect_hostname(bevs, NULL, AF_UNSPEC, conn->proxy->host, conn->proxy->port) == -1) {
    perror("DNS failure\n");
    exit(1);
  }
}


static void
read_client(struct bufferevent *bev, void *ctx) {
  conn_t *conn = ctx;

  if (conn->be_server != NULL) {
    //connected, foward data no matter to proxy or server

    bufferevent_read_buffer(bev, bufferevent_get_output(conn->be_server));
    return;
  }

  /* the first time talk to client
find out requested url and apply switching rules  */

  struct evbuffer *buffer = bufferevent_get_input(bev);

  struct evbuffer_ptr it;
  char *line;
  size_t line_len;

  it = evbuffer_search_eol(buffer, NULL, NULL, EVBUFFER_EOL_ANY);
  line_len = it.pos;
  line = malloc(it.pos+1);

  evbuffer_copyout(buffer, line, line_len);
  line[line_len] = '\0';

  sscanf(line, "%s %s", conn->method, conn->url);

  if (conn->url[0] == '/' ){
    rpc(bev, ctx, buffer);
    return;
  }

  conn->proxy = match_list(conn->url);

  free(line);

  if (conn->proxy != NULL) 

    read_client_http_proxy(bev, conn);

  else 
      
    read_client_direct(bev, conn);
  
}


static void 
client_event(struct bufferevent *bev, short e, void *ptr) {

  conn_t *conn = ptr;
  int fin = 0;
  if (e & BEV_EVENT_CONNECTED) {
    bufferevent_set_timeouts(bev, &config->timeout, &config->timeout);
  }
  else if (e & BEV_EVENT_ERROR){
    perror("Client Error\n");
    fin = 1;
  }
  if (e & BEV_EVENT_EOF) {
#ifdef DEBUG
    printf("Client EOF\n");
#endif
    fin = 1;
  }
  if (e & BEV_EVENT_TIMEOUT) {
    printf("CLIENT TIMEOUT \n");
    fin = 1;
  }
    
  //  printf("Close from client\n");
  if (fin) {
    // close connection
    if (conn->be_client != NULL) bufferevent_free(conn->be_client);
    if (conn->be_server != NULL) bufferevent_free(conn->be_server);
    free(conn);
  }
}

/* accept a connection */

static void
accept_conn_cb(struct evconnlistener *listener,
	       evutil_socket_t fd, struct sockaddr *address, int socklen,
	       void *ctx) {
  struct bufferevent *bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);

  conn_t *conn = malloc(sizeof(conn_t));
  conn->be_server = conn->be_client = NULL;
  
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
  sin.sin_addr.s_addr = htonl(0);
  sin.sin_port = htons(9999);

  listener = evconnlistener_new_bind(base, accept_conn_cb, NULL,
				     LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE, -1,
				     (struct sockaddr*)&sin, sizeof(sin));
  if(!listener) {
    perror("Unable to create listener");
    exit(1);
  }

  event_base_dispatch(base);
}
