#include "hashmap.h"
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
hashmap_insert (hashmap_t map, const char *key, const char *data)
{
        struct hashentry_s *ptr;
        unsigned int hash;
        char *key_copy;
        char *data_copy;

        assert (map != NULL);
        assert (key != NULL);
        assert (data != NULL);

        hash = hashfunc (key, map->size);

        key_copy = strdup (key);

        data_copy = strdup(data);

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

struct hashentry_s *
hashmap_find_head(hashmap_t map, const char *key) {
  unsigned int hash = hashfunc (key, map->size);

  return map->buckets[hash].head;

}

struct hashentry_s *
hashmap_find_next(struct hashentry_s *it, char *key) {
  struct hashentry_s *ent = it;

  while (1) {
    ent = ent->next;

    if (ent == NULL) return ent;

    if (strcasecmp(ent->key, key) == 0) 
      return ent;
  }
}
