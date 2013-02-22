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

#include <event2/http.h>
#include <event2/event.h>
#include <event.h>
#include <sys/queue.h>
#include <fnmatch.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "util.h"
#include "rpc.h"
#include "config.h"
#include "http.h"



static void 
get_proxies(struct evbuffer *rsps) {
  struct proxy_t *it = config->proxy_h->head;
  
  while (it != NULL) {
    evbuffer_add_printf(rsps, "%s,%s:%d\n", it->name, it->host, it->port);
    it = it->next;
  }
}

static void
get_lists(struct evbuffer *rsps) {
  struct acl *it = config->acl_h->head;
  
  while (it != NULL) {
    evbuffer_add_printf(rsps, "%s,%s\n", it->name, it->proxy->name);
    it = it->next;
  }
}

static void
get_log(struct evbuffer *rsps) {
  char *log = get_file_path("log");
  FILE *fd = fopen(log, "r");

  if (fd == NULL) return;

  int lines = get_line_count(fd);
  int start = lines - 50;

  if (start < 0) start = 0;

  int i; char buf[MAX_URL_LEN];

  for (i = 0; i < start; i++) 
    fgets(buf, MAX_URL_LEN, fd);


  while (fgets(buf, MAX_URL_LEN, fd) != NULL)
    evbuffer_add_printf(rsps, "%s", buf);

  fclose(fd);
}

static void
rm_log(struct evbuffer *rsps) {
  char *log = get_file_path("log");
  remove(log);

  evbuffer_add_printf(rsps, "OK");
}

static void
query(struct evbuffer *rsps, char *url) 
{
  struct acllist *al = config->acl_h;
  struct acl *node = al->head;

  char *domain = get_domain(url);

  if (domain == NULL) return;

  while (node != NULL) {
      
    struct hashmap_s *map = node->data;

    struct hashentry_s *it = hashmap_find_head(map, domain);

    while (it != NULL) {
      if (strcasestr(url, it->data) != NULL 
	  || fnmatch(it->data, url, FNM_CASEFOLD) == 0) 

	evbuffer_add_printf(rsps, "%s|%s\n", node->name, it->data);

      it = hashmap_find_next(it, domain);
      
    }
    node = node->next;
  }

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

static int
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

    else if (strncmp(conn->url, "/query?", 7) == 0)
      query(rsps, evhttp_decode_uri(conn->url + 7));

    else if (strcmp(conn->url, "/getlog") == 0) 
      get_log(rsps);

  }
  else if (strcmp(conn->method, "POST") == 0) {

    struct evkeyvalq kv;
    struct evhttp_uri *uri = evhttp_uri_parse_with_flags(conn->url, 0);

    evhttp_parse_query_str(evhttp_uri_get_query(uri), &kv);

    char *cont;
    if (s->length) {
      cont = malloc(s->length+1);
      evbuffer_remove(s->cont, cont, s->length);
      cont[s->length] = 0;
    }

    const char *path = evhttp_uri_get_path(uri);

    if (strcmp(path, "/addrule") == 0 || strcmp(path, "/rmrule") == 0) {

      struct evkeyvalq kvc;
      evhttp_parse_query_str(cont, &kvc);
      
      char *list = evhttp_decode_uri(evhttp_find_header(&kvc, "list"));
      char *rule = evhttp_decode_uri(evhttp_find_header(&kvc, "rule"));

      if (get_domain(rule) == NULL) 
	evbuffer_add_printf(rsps, "Invalid rule.");
      
      else {
	if (strcmp(path, "/addrule") == 0)
	  update_rule(list, rule);
      
	else 
	  remove_rule(list, rule);

	evbuffer_add_printf(rsps, "OK");
      }
      free(list);
      free(rule);
    }
    else if (strcmp(path, "/flush") == 0) {
      flush_list();
      evbuffer_add_printf(rsps, "OK");
    }
    else if (strcmp(path, "/rmlog") == 0) {
      rm_log(rsps);
    }

    evhttp_uri_free(uri);
  }
  
  ret(conn->be_client, rsps);
  evbuffer_free(rsps);
  return 1;
}


void rpc(void *ctx) {

  http_ready_cb(handle_request, ctx);

}
