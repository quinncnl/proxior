#include "match.h"
#include <fnmatch.h>
#include <string.h>
#include "config.h"

int readln_pos;

static void
readline(char *buf, char *list) {
  int i = 0; 
  int s = strlen(list);
  while (readln_pos < s 
	 && list[readln_pos] != '\n') {
    buf[i] = list[readln_pos];
    i++; readln_pos++;
  }

  buf[i] = 0;
  readln_pos++;
}

struct proxy_t *
match_list(char *url) {
  // 2KB line should be enough
  char buf[2048];

  struct acllist *al = config->acl_h;
  struct acl *node = al->data;

#ifdef DEBUG
  printf("URL: %s ", url);
#endif

  do {
    readln_pos = 0;
    do {
      readline(buf, node->data);
      if (buf[0] == 0) break;

      if (strcasestr(url, buf) != NULL 
	  || !fnmatch(buf, url, FNM_CASEFOLD)) {

#ifdef DEBUG
	printf(" Matched: %s\n", url, buf);
#endif
	return node->proxy;
      }
    } while (1);
    node = node->next;
  } while (node != NULL);


#ifdef DEBUG
  printf("Not Matched\n");
#endif
  return config->default_proxy;
}

