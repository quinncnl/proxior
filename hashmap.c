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

#include "hashmap.h"
#include "util.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>

static unsigned int hashfunc(const char *str, size_t size)
{
  unsigned int hash = 0;
  unsigned int x = 0; 

  while (*str)  {

      hash = (hash << 4) + (*str++);

      if ((x = hash & 0xF0000000L) != 0) {

	  hash ^= (x >> 24);

	  hash &= ~x;

	}
    }

  return (hash & 0x7FFFFFFF) % size;
}

struct hashmap_s *
hashmap_create (unsigned int nbuckets) {
  struct hashmap_s *ptr;

  ptr = malloc (sizeof (struct hashmap_s));
  if (!ptr)
    return NULL;

  ptr->size = nbuckets;
  ptr->buckets = (struct hashbucket_s *) calloc (nbuckets, sizeof (struct hashbucket_s));

  if (!ptr->buckets) {
    free (ptr);
    return NULL;
  }

  ptr->end_iterator = 0;

  return ptr;
}

int
hashmap_insert (hashmap_t map, char *rule)
{
        struct hashentry_s *ptr;
        unsigned int hash;
        char *key_copy;
        char *data_copy;

	char *key = get_domain(rule);

        hash = hashfunc (key, map->size);

        key_copy = strdup (key);

        data_copy = strdup(rule);

        ptr = (struct hashentry_s *) malloc (sizeof (struct hashentry_s));
        if (!ptr) {
                free (key_copy);
                free (data_copy);
                return -1;
        }

        ptr->key = key_copy;
        ptr->data = data_copy;

        /*
         * Now add the entry to the end of the bucket chain.
         */
        ptr->next = NULL;
        ptr->prev = map->buckets[hash].tail;
        if (map->buckets[hash].tail)
                map->buckets[hash].tail->next = ptr;

        map->buckets[hash].tail = ptr;
        if (!map->buckets[hash].head)
                map->buckets[hash].head = ptr;

        map->end_iterator++;
        return 0;
}

/* Find the first key match. */

struct hashentry_s *
hashmap_find_head(hashmap_t map, const char *key) {
 
 unsigned int hash = hashfunc (key, map->size);
 struct hashentry_s *it = map->buckets[hash].head;

 while (it && strcmp(it->key, key)) {
   it = it->next;
 }
 
 return it;
}

struct hashentry_s *
hashmap_find_next(struct hashentry_s *it, const char *key) {
  struct hashentry_s *ent = it;

  while (ent) {
    if (ent->next == NULL) return NULL;

    ent = ent->next;

    if (strcasecmp(ent->key, key) == 0) 
      return ent;
  }

  return ent;
}

void
hashmap_remove (hashmap_t map, const char *rule) 
{
  
  struct hashentry_s *it, *next;
  char *domain = get_domain(rule);

  unsigned int hash = hashfunc (domain, map->size);

  it = map->buckets[hash].head;

  while (it) {

    if (strcasecmp(it->key, domain) ||
	strcasecmp(it->data, rule)) {
      it = it->next;
      continue;
    }

    next = it->next;

    if (it->prev) 
      it->prev->next = it->next;

    if (it->next)
      it->next->prev = it->prev;

    if (map->buckets[hash].head == it)
      map->buckets[hash].head = it->next;

    if (map->buckets[hash].tail == it)
      map->buckets[hash].tail = it->prev;

    free(it->key);
    free(it->data);
    free(it);

    it = next;
  }

}
