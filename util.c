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

void urldecode2(char *dst, const char *src)
{
        char a, b;
        while (*src) {
                if ((*src == '%') &&
                    ((a = src[1]) && (b = src[2])) &&
                    (isxdigit(a) && isxdigit(b))) {
                        if (a >= 'a')
                                a -= 'A'-'a';
                        if (a >= 'A')
                                a -= ('A' - 10);
                        else
                                a -= '0';
                        if (b >= 'a')
                                b -= 'A'-'a';
                        if (b >= 'A')
                                b -= ('A' - 10);
                        else
                                b -= '0';
                        *dst++ = 16*a+b;
                        src+=3;
                } else {
                        *dst++ = *src++;
                }
        }
        *dst++ = '\0';
}
