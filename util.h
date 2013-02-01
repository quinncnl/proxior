#ifndef _UTIL_H_
#define _UTIL_H_

#include "common.h"

struct parsed_url {
  int port;
  char *url;
  char *host;
};

struct parsed_url *simple_parse_url(char *ori_url);

#endif /* _UTIL_H_ */
