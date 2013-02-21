#ifndef _SOCKS_H_
#define _SOCKS_H_


struct socksctx {
  short phase;
  char *host;
  unsigned short port;
  void (*callback)(void *);
  void *ctx;
};

struct bufferevent * 
socks_connect(char *host, unsigned short port, void (*callback)(void *), void *conn);

#endif /* _SOCKS_H_ */
