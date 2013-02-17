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
  along with Foobar.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "match.h"
#include <fnmatch.h>
#include <string.h>
#include "config.h"
#include "util.h"

struct proxy_t *
match_list(char *url) {

  struct acllist *al = config->acl_h;
  struct acl *node = al->data;

  char *domain = get_domain(url);
  while (node != NULL) {
      
    struct hashmap_s *map = node->data;

    struct hashentry_s *it = hashmap_find_head(map, domain);

    while (it != NULL) {
      printf("matching: %s\n", it->data);

      if (strcasestr(url, it->data) != NULL 
	  || fnmatch(it->data, url, FNM_CASEFOLD) == 0) 

	return node->proxy;

      it = hashmap_find_next(it, domain);
      
    }
    node = node->next;
  }
  //puts("no matched");
  return config->default_proxy;
}

