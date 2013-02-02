#include "rpc.h"
#include "config.h"

void get_proxies() {

}

void get_lists() {

}

void rpc(struct bufferevent *bev, void *ctx, struct evbuffer *buffer) {
  char *buf; size_t n;
  while ((buf = evbuffer_readln(buffer, &n, EVBUFFER_EOL_CRLF)) != NULL ) {
    printf("%s\n", buf);
  }
  char cont[] = "<html><head><title>HELLO</title></head>"
    "<body>WORLD</body></html>";
  //char str[5];

  evbuffer_add_printf(bufferevent_get_output(bev), 
		      "HTTP/1.1 200 OK\r\n"
		      "Content-Type: text/html\r\n"
		      "Content-Length: %d \r\n\r\n", (int) strlen(cont));

  evbuffer_add_printf(bufferevent_get_output(bev), cont, NULL);

  bufferevent_flush(bev, EV_WRITE, BEV_FLUSH|BEV_NORMAL);
}
