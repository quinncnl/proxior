#include "rpc.h"
#include "config.h"
#include "http.h"
#include "util.h"
#include <string.h>

static void 
get_proxies(char *rsps) {
  struct proxy_t *it = config->proxy_h->data;
  
  char *pos = rsps;
  while (it != NULL) {
    pos += sprintf(pos, "%s,%s:%d\n", it->name, it->host, it->port);
    it = it->next;
  }
}

static void 
get_lists(char rsps[]) {
  struct acl *it = config->acl_h->data;
  
  char *pos = rsps;

  while (it != NULL) {
    pos += sprintf(pos, "%s,%s\n", it->name, it->proxy->name);
    it = it->next;
  }
}

static void
get_list(char rsps[], char *listname) {
  struct acl *it = config->acl_h->data;

  while (it != NULL) {
    if (strcmp(listname, it->name) == 0) break;
    it = it->next;
  }

  sprintf(rsps, "%s", it->data);
}


static void
ret(struct bufferevent *bev, char *rsps) {
  struct evbuffer *output = bufferevent_get_output(bev);
  evbuffer_add_printf(output,
		      "HTTP/1.1 200 OK\r\n"
		      "Content-Type: text/html\r\n"
		      "Access-Control-Allow-Origin: *\r\n"
		      "Cache-Control: no-cache\r\n"
		      "Content-Length: %d\r\n\r\n", (int) strlen(rsps));

  evbuffer_add_printf(output, rsps, NULL);

  bufferevent_flush(bev, EV_WRITE, BEV_FLUSH|BEV_NORMAL);
}

void rpc(struct bufferevent *bev, void *ctx, struct evbuffer *buffer) {

  conn_t *conn = ctx;
  char rsps[102400];


  if (strcmp(conn->method, "GET") == 0) {

    evbuffer_drain(buffer, evbuffer_get_length(buffer));
  
    if (strcmp(conn->url, "/getproxies") == 0) 
      get_proxies(rsps);
    else if (strcmp(conn->url, "/getlists") == 0) 
      get_lists(rsps);
    else if (strncmp(conn->url, "/getlist?", 9) == 0)
      get_list(rsps, conn->url+9);

    ret(bev, rsps);
  }
  else if (strcmp(conn->method, "POST") == 0) {
    char *line;
    char header[200], header_v[200];
    int size;
    char *content;
   
    while ((line = evbuffer_readln(buffer, NULL, EVBUFFER_EOL_ANY)) != NULL) {
      sscanf(line, "%s %s", header, header_v);

      if (strcmp(header, "Content-Length:") == 0) {
	size = atoi(header_v);
	//printf("size: %d\n", size);
	break;
      }
    }
    
    while ((line = evbuffer_readln(buffer, NULL, EVBUFFER_EOL_ANY)) != NULL);

    content = malloc(size);
    printf("read: %d\n", evbuffer_remove(buffer, content, size));

    if (strncmp(conn->url, "/updatelist?", 12) == 0) {
      char *listname = conn->url+12;

      char *post_cont;
      char *post_use;
      strtok(content, "=");
      post_use = strtok(NULL, "&");
      strtok(NULL, "=");
      post_cont = strtok(NULL, "");
      char *decoded = malloc(strlen(post_cont)+1);
      urldecode2(decoded, post_cont);
      update_list(decoded, post_use, listname);
    }
  }

}
