#include <event2/http.h>
#include <event2/event.h>
#include <event.h>
#include <sys/queue.h>

#include "rpc.h"
#include "config.h"
#include "http.h"


struct post {
  int length, read;
  int is_cont;
  struct evbuffer *cont;
};

static void 
get_proxies(struct evbuffer *rsps) {
  struct proxy_t *it = config->proxy_h->data;
  
  while (it != NULL) {
    evbuffer_add_printf(rsps, "%s,%s:%d\n", it->name, it->host, it->port);
    it = it->next;
  }
}

static void 
get_lists(struct evbuffer *rsps) {
  struct acl *it = config->acl_h->data;
  
  while (it != NULL) {
    evbuffer_add_printf(rsps, "%s,%s\n", it->name, it->proxy->name);
    it = it->next;
  }
}

static void
get_list(struct evbuffer *rsps, char *listname) {
  struct acl *it = config->acl_h->data;

  while (it != NULL) {
    if (strcmp(listname, it->name) == 0) break;
    it = it->next;
  }

  evbuffer_add_printf(rsps, "%s", it->data);
}


static void
ret(struct bufferevent *bev, struct evbuffer *rsps) {
  struct evbuffer *output = bufferevent_get_output(bev);
  evbuffer_add_printf(output,
		      "HTTP/1.1 200 OK\r\n"
		      "Content-Type: text/html\r\n"
		      "Access-Control-Allow-Origin: *\r\n"
		      "Connection: close\r\n"
		      "Cache-Control: no-cache\r\n"
		      "Content-Length: %d\r\n\r\n", (int) evbuffer_get_length(rsps));
  
  evbuffer_remove_buffer(rsps, output, 102400);
}

void rpc(struct bufferevent *bev, void *ctx, struct evbuffer *buffer) {

  conn_t *conn = ctx;
  struct evbuffer *rsps = evbuffer_new();

  if (strcmp(conn->method, "GET") == 0) {
    // we don't care about the header
    evbuffer_drain(buffer, evbuffer_get_length(buffer));
  
    if (strcmp(conn->url, "/getproxies") == 0) 
      get_proxies(rsps);
    else if (strcmp(conn->url, "/getlists") == 0) 
      get_lists(rsps);
    else if (strncmp(conn->url, "/getlist?", 9) == 0)
      get_list(rsps, conn->url+9);

  }
  else if (strcmp(conn->method, "POST") == 0) {
    struct post *p = conn->rpc;
    if (p == NULL) 
      conn->rpc = p = calloc(sizeof(struct post), 1);
    

    if (!p->is_cont) {
      char *line;
      char header[200], header_v[200];
      int size;
   
      while ((line = evbuffer_readln(buffer, NULL, EVBUFFER_EOL_CRLF)) != NULL) {
	sscanf(line, "%s %s", header, header_v);
	free(line);
	if (strcmp(header, "Content-Length:") == 0) {
	  size = atoi(header_v);
	  printf("size: %d\n", size);
	  p->length = size;
	  break;
	}
      }
    
      while ((line = evbuffer_readln(buffer, NULL, EVBUFFER_EOL_CRLF)) != NULL) {
	if (strlen(line) == 0) {
	  p->is_cont = 1;
	  free(line);
	  break;
	}
	free(line);
      }
    }

    if (p->is_cont) {
      if (p->cont == NULL) p->cont = evbuffer_new();

      int read = evbuffer_remove_buffer(buffer, p->cont, p->length);
      p->read += read;
      printf("read: %d\n", read);
    }

    if (p->read == p->length) {
      struct evkeyvalq kv;
      struct evhttp_uri *uri = evhttp_uri_parse_with_flags(conn->url, 0);

      evhttp_parse_query_str(evhttp_uri_get_query(uri), &kv);

      char *cont = (char *) evbuffer_pullup(p->cont, p->length);

      if (strcmp(evhttp_uri_get_path(uri), "/updatelist") == 0) {
	const char *listname = evhttp_find_header(&kv, "list");

	struct evkeyvalq kvc;
	evhttp_parse_query_str(cont, &kvc);
      
	char *newcont = evhttp_decode_uri(evhttp_find_header(&kvc, "data"));
	update_list(newcont, listname);
	evbuffer_add_printf(rsps, "OK");

	free(newcont);

      }

      evbuffer_free(p->cont);
    }
  
  }
  ret(bev, rsps);
  evbuffer_free(rsps);
 
  free(conn->rpc);
}
