/*

  Copyright (c) 2013 by Clear Tsai

  Proxior is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  any later version.

  Proxior is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Proxior.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "util.h"
#include "common.h"

/* Parse HTTP URL
 * This will make a copy of the original url. 
 * Remember to free it.
*/

struct parsed_url *simple_parse_url(char *ori_url) {

  if (strlen(ori_url) == 0) return NULL;

  char *s_port, *domain;
  struct parsed_url *ret = malloc(sizeof(struct parsed_url));
  char *url = strdup(ori_url);

  strtok(url, "//");
  domain = strtok(NULL, "/");

  if (domain == NULL) 
    domain = url;

  domain = strtok(domain, ":");

  s_port = strtok(NULL, "");
  if (s_port == NULL) ret->port = 80;
  else ret->port = atoi(s_port);

  ret->host = strdup(domain);

  free(url);
  //  printf("url created: %s\n", domain);
  return ret;
}

void
free_parsed_url (struct parsed_url *url) {
  //  printf("url freed: %s\n", url->host);
  if (url == NULL) return;
  free(url->host);
  free(url);
}

char *get_domain(const char *ori) {
  static char url[MAX_URL_LEN];
  strcpy(url, ori);

  char *s = strstr(url, "//\0");
  if (s == NULL) s = url;
  else s += 2;

  s = strtok(s, "/");
  char *s2 = strrchr(s, '.');

  if (s2 == NULL) 
    return NULL;

  s2--;

  while (strncmp(s2, ".", 1) && s2 > s) s2--;
  if (*s2 == '.')  s2++;

  return strtok(s2, ":");
}

int get_line_count(FILE *fh) {
  int lines = 0;
  while (EOF != (fscanf(fh, "%*[^\n]"), fscanf(fh, "%*c"))) 
    ++lines;

  rewind(fh);

  return lines;
}

/* Written on Mon Feb 25 2013
 * Replace of fnmatch
 */

int astermatch (char *url, char *pattern) {

  char *u = url, *p = pattern;

  if (!*p) return 0;
  while (*p) {

    if (*p == '*') {
      if (!*u && !*(p + 1)) return 0;

      while (*u) 
	if (astermatch(u, p + 1))
	  u++;
	else return 0;

      return 1;
    }
    else {
      if (*u && *p == *u) {
	p++;
	u++;
      }
      else return 1;
    }
  }

  if (*p == *u) return 0;
  return 1;

}
