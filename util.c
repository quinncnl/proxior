#include "util.h"

/* Parse HTTP URL
 * This will make a copy of the original url. 
 * Remember to free it.
*/

struct parsed_url *simple_parse_url(char *ori_url) {

  char *s_port, *domain;
  struct parsed_url *ret = malloc(sizeof(struct parsed_url));
  ret->url = malloc(strlen(ori_url)+1);

  strcpy(ret->url, ori_url);

  strtok(ret->url, "//");
  domain = strtok(NULL, "/");
  domain = strtok(domain, ":");
  s_port = strtok(NULL, "");
  if (s_port == NULL) ret->port = 80;
  else ret->port = atoi(s_port);

  ret->host = malloc(strlen(domain)+1);
  strcpy(ret->host, domain);
  return ret;
}
