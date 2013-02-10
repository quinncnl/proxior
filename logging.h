#ifndef _LOGGING_H_
#define _LOGGING_H_

#include "config.h"
#include "common.h"

void open_log();

void close_log();

void log_error(int code, char *string, char *url, struct proxy_t *proxy);


#endif /* _LOGGING_H_ */
