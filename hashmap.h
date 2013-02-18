#ifndef _HASHMAP_H_
#define _HASHMAP_H_

#include <sys/types.h>

struct hashentry_s {
        char *key;
        char *data;

        struct hashentry_s *prev, *next;
};

struct hashbucket_s {
        struct hashentry_s *head, *tail;
};

typedef struct hashmap_s *hashmap_t;
typedef int hashmap_iter;

struct hashmap_s {
        unsigned int size;
        hashmap_iter end_iterator;

        struct hashbucket_s *buckets;
};

struct hashmap_s *
hashmap_create (unsigned int nbuckets) ;

int
hashmap_insert (hashmap_t map, char *rule);

struct hashentry_s *
hashmap_find_head(hashmap_t map, const char *key);

struct hashentry_s *
hashmap_find_next(struct hashentry_s *ent, const char *key);

void
hashmap_remove (hashmap_t map, const char *rule);

#endif /* _HASHMAP_H_ */
