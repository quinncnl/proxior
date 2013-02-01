#ifndef _HTTP_H_
#define _HTTP_H_

#include "common.h"
#include "logging.h"
#include "config.h"

extern conf *config;

#define CONNECT_DIRECT 1
#define CONNECT_HTTP_PROXY 2

void start();

#endif /* _HTTP_H_ */
