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

#include <event2/http.h>
#include <event2/event.h>
#include <event.h>
#include <sys/queue.h>

#include "rpc.h"
#include "config.h"
#include "http.h"



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

static void
handle_request(void *ctx) {
  conn_t *conn = ctx;
  struct evbuffer *rsps = evbuffer_new();
  struct state *s = conn->state;

  if (strcmp(conn->method, "GET") == 0) {
    // we don't care about the header
    evbuffer_drain(s->header, evbuffer_get_length(s->header));
  
    if (strcmp(conn->url, "/getproxies") == 0) 
      get_proxies(rsps);
    else if (strcmp(conn->url, "/getlists") == 0) 
      get_lists(rsps);
    else if (strncmp(conn->url, "/getlist?", 9) == 0)
      get_list(rsps, conn->url+9);

  }
  else if (strcmp(conn->method, "POST") == 0) {

    struct evkeyvalq kv;
    struct evhttp_uri *uri = evhttp_uri_parse_with_flags(conn->url, 0);

    evhttp_parse_query_str(evhttp_uri_get_query(uri), &kv);

    char *cont = malloc(s->length+1);
    evbuffer_remove(s->cont, cont, s->length);
    cont[s->length] = 0;

    if (strcmp(evhttp_uri_get_path(uri), "/updatelist") == 0) {
      const char *listname = evhttp_find_header(&kv, "list");

      struct evkeyvalq kvc;
      evhttp_parse_query_str(cont, &kvc);
      
      char *newcont = evhttp_decode_uri(evhttp_find_header(&kvc, "data"));
      update_list(newcont, listname);
      evbuffer_add_printf(rsps, "OK");

      free(newcont);
    }

    evhttp_uri_free(uri);
  }
  
  ret(conn->be_client, rsps);
  evbuffer_free(rsps);

}


void rpc(void *ctx) {

  http_ready_cb(handle_request, ctx);

}
