#ifndef _COMMON_H_
#define _COMMON_H_

#define MAX_URL_LEN 3600

#include <stdio.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <string.h>
#include <stdlib.h>

#include <errno.h>
#include <event2/util.h>
#include <event2/event.h>
#include <stdarg.h>

#include <arpa/inet.h>

/* We need one event base */
struct event_base *base;

#endif /* _COMMON_H_ */
