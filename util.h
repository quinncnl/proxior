#ifndef _UTIL_H_
#define _UTIL_H_

#include "common.h"

struct parsed_url {
  unsigned short port;
  char *url;
  char *host;
};

struct parsed_url *simple_parse_url(char *ori_url);

void
free_parsed_url (struct parsed_url *url);

char *get_domain(const char *url);

int get_line_count(FILE *fh) ;

int astermatch (char *url, char *pattern);

#endif /* _UTIL_H_ */
