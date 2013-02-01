#ifndef _RPC_H_
#define _RPC_H_

#include "common.h"
void get_proxies();

void get_lists();

void rpc(struct bufferevent *bev, void *ctx, struct evbuffer *buffer);

#endif /* _RPC_H_ */

