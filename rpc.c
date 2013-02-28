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
#include <event2/http.h>
#include <event.h>

#include "util.h"
#include "rpc.h"
#include "config.h"
#include "http.h"

static void
get_trylist(struct evbuffer *rsps) {
  struct rulelist *rl = get_rulelist("trylist");

  struct hashentry_s *it;
  struct hashmap_s *map = rl->data;
  int i;

  for (i = 0; i < map->size; i++) {
      
    it = map->buckets[i].head;
    while (it) {
      evbuffer_add_printf(rsps, "%s\n", it->data);
      it = it->next;
    }
  }
}

static void 
get_proxies(struct evbuffer *rsps) {
  struct proxy_t *it = cfg.proxy_head; 

  while (it != NULL) {
    evbuffer_add_printf(rsps, "%s,%s:%d\n", it->name, it->host, it->port);
    it = it->next;
  }
}

static void
get_lists(struct evbuffer *rsps) {
  struct rulelist *it = cfg.rulelist_head;
  
  while (it != NULL) {
    if (strcmp(it->name, "trylist")) 
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
  struct rulelist *node = cfg.rulelist_head;

  char *domain = get_domain(url);

  if (domain == NULL) return;

  while (node != NULL) {
      
    struct hashmap_s *map = node->data;

    struct hashentry_s *it = hashmap_find_head(map, domain);

    while (it != NULL) {
      if (strstr(url, it->data) != NULL 
	  || astermatch(url, it->data) == 0) 

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

static void
handle_request(void *ctx) {
  conn_t *conn = ctx;
  struct evbuffer *rsps = evbuffer_new();
  struct state *s = conn->state;

  if (strcmp(conn->method, "GET") == 0) {

    if (strcmp(conn->url, "/getproxies") == 0) 
      get_proxies(rsps);

    else if (strcmp(conn->url, "/getlists") == 0) 
      get_lists(rsps);

    else if (strncmp(conn->url, "/query?", 7) == 0)
      query(rsps, evhttp_decode_uri(conn->url + 7));

    else if (strcmp(conn->url, "/getlog") == 0) 
      get_log(rsps);

    else if (strcmp(conn->url, "/gettrylist") == 0) 
      get_trylist(rsps);

    else if (strcmp(conn->url, "/getversion") == 0) 
      evbuffer_add_printf(rsps, VERSION);

  }
  else if (strcmp(conn->method, "POST") == 0) {

    struct evkeyvalq kv;
    struct evhttp_uri *uri = evhttp_uri_parse_with_flags(conn->url, 0);

    evhttp_parse_query_str(evhttp_uri_get_query(uri), &kv);

    char *cont;
    if (s->length) 
      cont = s->body;

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

      free(cont);
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

}


void rpc(struct bufferevent*bev, void *ctx) {

  conn_t *conn = ctx;
  struct state *s = conn->state; 
  struct evbuffer *buffer = bufferevent_get_input(conn->be_client);


  char *line;
  char header[64], header_v[2500];
  if (s == NULL) {
    conn->state = s = calloc(sizeof(struct state), 1);
    s->pos = STATE_HEADER;
  }

  if (s->pos == STATE_HEADER) {
    while ((line = evbuffer_readln(buffer, NULL, EVBUFFER_EOL_CRLF)) != NULL) {

      if (strlen(line) == 0) {

	free(line);
	s->pos = STATE_BODY;
	break;
      }

      sscanf(line, "%s %s", header, header_v);
      
      if (strcmp(header, "Content-Length:") == 0) 
	s->length = atoi(header_v);

      free(line);
    }
  }

  if (s->pos == STATE_BODY) {
    if (s->length == 0) goto end; 

    s->body = malloc(s->length + 1);
    evbuffer_remove(buffer, s->body, s->length);
    s->body[s->length] = 0;

  }
  else return;

 end:
  handle_request(conn);
}
