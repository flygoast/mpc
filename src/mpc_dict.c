/* It came from Redis. I rewrote it in libutils to expect that 
   it can be used in my future project. 
                                -- flygoast(flygoast@126.com)

   This file implements in memory hash tables with insert/delete/
   replace/find/get-random-element operations. Hash table will
   auto resize if needed. Tables of power of two in size are used,
   collisions are handled by chaining. See the source code for 
   more information...
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <limits.h>
#include <sys/time.h>
#include "dict.h"

/* Using dict_enable_resize()/dict_disable_resize(), we make possible
   to enable/disable resizing of the hash table as needed. This is 
   very important for Redis, as we use copy-on-write and don't want
   to move too much memory around when there is a child performing
   saving operations. Note that even when dict_can_resize is set to
   0, not all resizes are prevented: a hash table is still allowed
   to grow if the ratio between the number of elements and the buckets
   > dict_force_resize_ratio. */
static int dict_can_resize = 1;
static unsigned int dict_force_resize_ratio = 5;

/* ---------------- private function prototypes ----------------- */
static void _dict_init(dict *d, dict_type *type, void *privdata);
static void _ht_init(dictht *ht);
static int _dict_expand_if_needed(dict *d);
static unsigned long _dict_next_power(unsigned long size);
static int _dict_key_index(dict *d, const void *key);
static void _dict_rehash_step(dict *d);
static int _dict_clear(dict *d, dictht *ht);
static int _dict_expand(dict *d, unsigned long size);

/* ---------------------- hash functions --------------------- */

/* Thomas Wang's 32 bit Mix Function */
unsigned int dict_int_hash(unsigned int key) {
    key += ~(key << 15);
    key ^= (key >> 10);
    key += (key << 3);
    key ^= (key >> 6);
    key += ~(key << 11);
    key ^= (key >> 16);
    return key;
}

/* Identity hash function for integer keys */
unsigned int dict_identity_hash(unsigned int key) {
    return key;
}

/* Generic hash function (a popular one from Bernstein).
   I tested a few and this was the best.(from Redis's author) */
unsigned int dict_gen_hash(const unsigned char *buf, int len) {
    unsigned int hash = 5381;

    while (len--)
        hash = ((hash << 5) + hash) + (*buf++); /* hash * 33 + c */
    return hash;
}

/* And a case insensitive version */
unsigned int dict_gen_case_hash(const unsigned char *buf, int len) {
    unsigned int hash = 5381;

    while (len--)
        hash = ((hash << 5) + hash) + (tolower(*buf++));
    return hash;
}

/* ------------------ API implementation ----------------*/

/* Create a dict. */
dict *dict_create(dict_type *type, void *privdata) {
    dict *d = malloc(sizeof(*d));
    if (!d) {
        return NULL;
    }
    _dict_init(d, type, privdata);
    return d;
}

/* Perform N steps of incremental rehashing. Returns 1 if there are
   still keys to move from the old to the new hash table, otherwise
   0 is returned. Note that a rehashing step consists in moving a 
   bucket (that may have more than one key as we use chaining) from
   the old to the new hashing table. */
int dict_rehash(dict *d, int n) {
    if (!DICT_IS_REHASHING(d)) { /* Not in rehashing status. */
        return 0;
    }

    while (n--) {
        dict_entry *de, *nextde;
        
        /* Check if we already rehashed the whole table ... */
        if (d->ht[0].used == 0) {
            free(d->ht[0].table);
            /* Structure assignments have been supported since C90. */
            d->ht[0] = d->ht[1]; 
            _ht_init(&d->ht[1]);
            d->rehashidx = -1; /* Exit rehashing status. */
            return 0;
        }

        /* Note that rehashidx can't overflow as we are sure there
           are more elements because ht[0].used != 0 */
        while (d->ht[0].table[d->rehashidx] == NULL) {
            ++d->rehashidx;
        }

        /* Move all the keys in this bucket from the old to the new
           hash table */
        de = d->ht[0].table[d->rehashidx];
        while (de) {
            unsigned int h;
            nextde = de->next;
            /* Get the index in the new hash table */
            h = DICT_HASH_KEY(d, de->key) & d->ht[1].sizemask;
            de->next = d->ht[1].table[h];
            d->ht[1].table[h] = de;
            d->ht[0].used--;
            d->ht[1].used++;
            de = nextde;
        }
        d->ht[0].table[d->rehashidx] = NULL;
        ++d->rehashidx;
    }
    return 1;
}

/* Clear & Release the hash table */
void dict_release(dict *d) {
    _dict_clear(d, &d->ht[0]);
    _dict_clear(d, &d->ht[1]);
    free(d);
}

void dict_empty(dict *d) {
    _dict_clear(d, &d->ht[0]);
    _dict_clear(d, &d->ht[1]);
    d->rehashidx = -1;
    d->iterators = 0;
}

/* Resize the table to the minimal size that contains all the
   elements, but with the invariant of a USER/BUCKETS ratio
   near to <= 1 */
int dict_resize(dict *d) {
    int minimal;

    if (!dict_can_resize || DICT_IS_REHASHING(d)) {
        return DICT_ERR;
    }

    minimal = d->ht[0].used;
    if (minimal < DICT_HT_INITIAL_SIZE)
        minimal = DICT_HT_INITIAL_SIZE;
    return _dict_expand(d, minimal);
}

long long time_in_milliseconds(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (((long long)tv.tv_sec) * 1000) + (tv.tv_usec / 1000);
}

/* Rehash for an amount of time between ms milliseconds and 
   (ms + 1) milliseconds */
int dict_rehash_milliseconds(dict *d, int ms) {
    long long start = time_in_milliseconds();
    int rehashes = 0;

    while (dict_rehash(d, 100)) {
        rehashes += 100;
        if (time_in_milliseconds() - start > ms) {
            break;
        }
    }
    return rehashes;
}

/* Add an element to the target dict. */
int dict_add(dict *d, void *key, void *val) {
    int         index;
    dict_entry  *entry;
    dictht      *ht;

    if (DICT_IS_REHASHING(d)) { /* In rehashing status. */
        _dict_rehash_step(d);
    }

    /* Get the index of the new element, or -1 if the element
       already exists. */
    if ((index = _dict_key_index(d, key)) == -1) {
        return DICT_ERR;
    }

    /* Allocates the meory and stores key. */
    ht = DICT_IS_REHASHING(d) ? &d->ht[1] : &d->ht[0];
    entry = malloc(sizeof(*entry));
    if (!entry) {
        assert(0);
        return DICT_ERR;
    }
    entry->next = ht->table[index];
    ht->table[index] = entry;
    ht->used++;

    /* Set the hash entry fields. */
    DICT_SET_HASH_KEY(d, entry, key);
    DICT_SET_HASH_VAL(d, entry, val);
    return DICT_OK;
}

/* Add an element, discarding the old if the key already exists.
   Return 1 if the key was added from scratch, 0 if there was
   already an element with such key and dict_replace() just
   performed a value update operation. */
int dict_replace(dict *d, void *key, void *val) {
    dict_entry *entry, auxentry;

    /* Try to add the element. If the key does not exists, dict_add
       will succeed. */
    if (dict_add(d, key, val) == DICT_OK) {
        return 1;
    }
    /* It already exists, get the entry. */
    entry = dict_find(d, key);
    assert(entry != NULL);
    /* Free the old value and set the new one. */
    /* Set the new value and free the old one. Note that it is 
       important to do that in this order, as the value may just
       be exactly the same as the previous one. In this context,
       think to reference counting, you want to increment(set), 
       and then decrement(free), and not the reverse. */
    auxentry = *entry;
    DICT_SET_HASH_VAL(d, entry, val);
    DICT_FREE_ENTRY_VAL(d, &auxentry);
    return 0;
}

/* Search and remove an element */
static int dict_generic_delete(dict *d, const void *key, int nofree) {
    unsigned int h, idx;
    dict_entry *de, *prevde;
    int table;

    if (d->ht[0].size == 0) {
        return DICT_ERR;
    }

    if (DICT_IS_REHASHING(d)) {
        _dict_rehash_step(d);
    }
    
    h = DICT_HASH_KEY(d, key);

    for (table = 0; table <= 1; ++table) {
        idx = h & d->ht[table].sizemask;
        de = d->ht[table].table[idx];
        prevde = NULL;
        while (de) {
            if (DICT_CMP_HASH_KEYS(d, key, de->key)) {
                /* Unlink the element from the list */
                if (prevde) {
                    prevde->next = de->next;
                } else {
                    d->ht[table].table[idx] = de->next;
                }
                if (!nofree) {
                    DICT_FREE_ENTRY_KEY(d, de);
                    DICT_FREE_ENTRY_VAL(d, de);
                }
                free(de);
                d->ht[table].used--;
                return DICT_OK;
            }
            prevde = de;
            de = de->next;
        }
        if (!DICT_IS_REHASHING(d)) {
            break;
        }
    }
    return DICT_ERR; /* not found */
}

int dict_delete(dict *d, const void *key) {
    return dict_generic_delete(d, key, 0);
}

int dict_delete_nofree(dict *d, const void *key) {
    return dict_generic_delete(d, key, 1);
}

dict_entry *dict_find(dict *d, const void *key) {
    dict_entry *he;
    unsigned int h, idx, table;

    if (d->ht[0].size == 0) {
        return NULL; /* We don't have a table at all. */
    }

    if (DICT_IS_REHASHING(d)) {
        _dict_rehash_step(d);
    }

    h = DICT_HASH_KEY(d, key);
    for (table = 0; table <= 1; ++table) {
        idx = h & d->ht[table].sizemask;
        he = d->ht[table].table[idx];
        while (he) {
            if (DICT_CMP_HASH_KEYS(d, key, he->key)) {
                return he;
            }
            he = he->next;
        }

        if (!DICT_IS_REHASHING(d)) {
            return NULL;
        }
    }
    return NULL;
}

void *dict_fetch_value(dict *d, const void *key) {
    dict_entry *de;
    de = dict_find(d, key);
    return de ? DICT_GET_ENTRY_VAL(de) : NULL;
}

dict_iterator *dict_get_iterator(dict *d) {
    dict_iterator *iter = malloc(sizeof(*iter));
    if (!iter) {
        return NULL;
    }

    iter->d = d;
    iter->table = 0;
    iter->index = -1;
    iter->safe = 0;
    iter->entry = NULL;
    iter->next_entry = NULL;
    return iter;
}

dict_iterator *dict_get_safe_iterator(dict *d) {
    dict_iterator *i = dict_get_iterator(d);
    if (!i) {
        return NULL;
    }
    i->safe = 1;
    return i;
}

dict_entry *dict_next(dict_iterator *iter) {
    while (1) {
        if (iter->entry == NULL) {
            dictht *ht = &iter->d->ht[iter->table];
            if (iter->safe && iter->index == -1 && iter->table == 0) {
                iter->d->iterators++;
            }
            iter->index++;
            if (iter->index >= (signed int)ht->size) {
                if (DICT_IS_REHASHING(iter->d) && iter->table == 0) {
                    iter->table++;
                    iter->index = 0;
                    ht = &iter->d->ht[1];
                } else {
                    break;
                }
            }
            iter->entry = ht->table[iter->index];
        } else {
            iter->entry = iter->next_entry;
        }

        if (iter->entry) {
            /* We need to save the 'next' here, the iterator user
               may delete the entry we are returning. */
            iter->next_entry = iter->entry->next;
            return iter->entry;
        }
    }
    return NULL;
}

void dict_release_iterator(dict_iterator *iter) {
    if (iter->safe && !(iter->index == -1 && iter->table == 0)) {
        iter->d->iterators--;
    }
    free(iter);
}

/* Return a random entry from the hash table. Useful to implement
   randomized algorithms. */
dict_entry *dict_get_random_key(dict *d) {
    dict_entry *he, *orighe;
    unsigned int h;
    int listlen, listele;

    if (DICT_SIZE(d) == 0) {
        return NULL;
    }

    if (DICT_IS_REHASHING(d)) {
        _dict_rehash_step(d);
    }

    if (DICT_IS_REHASHING(d)) {
        do {
            h = random() % (d->ht[0].size + d->ht[1].size);
            he = (h >= d->ht[0].size) ? 
                d->ht[1].table[h - d->ht[0].size] : d->ht[0].table[h];
        } while (he == NULL);
    } else {
        do {
            h = random() & d->ht[0].sizemask;
            he = d->ht[0].table[h];
        } while (he == NULL);
    }

    /* Now we found a non empty bucket, but it is a linked list and
       we need to get a random element from the list. The only sane
       way to do so is counting the elements and select a random
       index. */
    listlen = 0;
    orighe = he;
    while (he) {
        he = he->next;
        listlen++;
    }
    listele = random() % listlen;
    he = orighe;
    while (listele--) {
        he = he->next;
    }
    return he;
}

/* ----------------------- private functions --------------------- */

/* Initialize the dict. */
static void _dict_init(dict *d, dict_type *type, void *privdata) {
    _ht_init(&d->ht[0]);
    _ht_init(&d->ht[1]);
    d->type = type;
    d->privdata = privdata;
    d->iterators = 0;
}

/* Reset an hashtable already initialized. */
static void _ht_init(dictht *ht) {
    ht->table = NULL;
    ht->size = 0;
    ht->sizemask = 0;
    ht->used = 0;
}

/* Destroy an entire dictionary */
static int _dict_clear(dict *d, dictht *ht) {
    unsigned long i;

    /* Free all the elements */
    for (i = 0; i < ht->size && ht->used > 0; ++i) {
        dict_entry *he, *nexthe;

        if ((he = ht->table[i]) == NULL) {
            continue;
        }
        while (he) {
            nexthe = he->next;
            DICT_FREE_ENTRY_KEY(d, he);
            DICT_FREE_ENTRY_VAL(d, he);
            free(he);
            ht->used--;
            he = nexthe;
        }
    }
    /* Free the table and the allocated cache structure */
    free(ht->table);
    /* Re-initialize the table */
    _ht_init(ht);
    return DICT_OK;
}

static int _dict_expand_if_needed(dict *d) {
    /* Incremental rehashing already in progress. Return. */
    if (DICT_IS_REHASHING(d)) {
        return DICT_OK;
    }

    /* If the hash table is empty, expand it to the initial size. */
    if (d->ht[0].size == 0) {
        return _dict_expand(d, DICT_HT_INITIAL_SIZE);
    }

    /* If we reached the 1:1 ratio, and we are allowed to resize the
       hash table(global setting) or we should avoid it but the ratio
       between elements/buckets is over the "safe" threshold, we 
       resize doubling the number of buckets. */
    if (d->ht[0].used >= d->ht[0].size && 
        (dict_can_resize || 
         d->ht[0].used / d->ht[0].size > dict_force_resize_ratio)) {
        return _dict_expand(d, ((d->ht[0].size > d->ht[0].used) ?
                                d->ht[0].size : d->ht[0].used) * 2);
    }
    return DICT_OK;
}

/* Our hash table capability is a power of two(2 ** n). */
static unsigned long _dict_next_power(unsigned long size) {
    unsigned long i = DICT_HT_INITIAL_SIZE;

    if (size >= LONG_MAX) {
        return LONG_MAX;
    }

    while (1) {
        if (i >= size)
            return i;
        i *= 2;
    }
}

/* Expand or create the hashtable */
static int _dict_expand(dict *d, unsigned long size) {
    dictht ht; /* the new hashtable */
    unsigned long realsize = _dict_next_power(size);

    /* the size is invalid if it is smaller than the number of 
       elements already inside the hashtable */
    if (DICT_IS_REHASHING(d) || d->ht[0].used > size) {
        return DICT_ERR;
    }

    /* Allocate the new hashtable and initialize all pointers to NULL */
    ht.size = realsize;
    ht.sizemask = realsize - 1;
    ht.table = calloc(sizeof(dict_entry*), realsize);
    if (!ht.table) {
        fprintf(stderr, "calloc failed");
        return DICT_ERR;
    }
    ht.used = 0;
    
    /* Is this the first initialization? If so it's not really a
       rehashing we just set the first hash table so that it can
       accept keys. */
    if (d->ht[0].table == NULL) {
        d->ht[0] = ht;
        return DICT_OK;
    }

    /* Prepare a second hash table for incremental rehashing */
    d->ht[1] = ht;
    d->rehashidx = 0; /* Transmit to rehashing status. */
    return DICT_OK;
}

/* Return the index of a free slot that can be populated with
   a hash entry for the given 'key'. If the key already
   exists, -1 is returned. Note that if we are in the process
   of rehashing the hash table, the index is always returned 
   in the context of the second(new) hash table. */
static int _dict_key_index(dict *d, const void *key) {
    unsigned int h, idx, table;
    dict_entry *he;

    /* Expand the hash table if needed. */
    if (_dict_expand_if_needed(d) == DICT_ERR) {
        /* I think you'll never get here! */
        assert(0);
        return -1;
    }

    /* Compute the key hash value. */
    h = DICT_HASH_KEY(d, key);
    for (table = 0; table <= 1; ++table) {
        idx = h & d->ht[table].sizemask;
        /* Search if this slot does not already contain the given key. */
        he = d->ht[table].table[idx];
        while (he) {
            /* The key has existed. */
            if (DICT_CMP_HASH_KEYS(d, key, he->key)) {
                return -1;
            }
            he = he->next;
        }
        if (!DICT_IS_REHASHING(d)) {
            break;
        }
    }
    return idx;
}

/* This function performs just a step of rehashing, and only if
   there are no safe iterators bound to our hash table. When we
   have iterators in the middle of a rehashing we can't mess with
   the two hash tables otherwise some element can be missed or
   duplicated.

   This function is called by common lookup or update operations
   in the dictionary so that the hash table automatically migrates
   from H1 to H2 while it is actively used. */
static void _dict_rehash_step(dict *d) {
    if (d->iterators == 0) {
        dict_rehash(d, 1);
    }
}


#define DICT_STATS_VECTLEN 50
static void _dict_print_stats_ht(dictht *ht) {
    unsigned long i, slots = 0, chainlen, maxchainlen = 0;
    unsigned long totchainlen = 0;
    unsigned long clvector[DICT_STATS_VECTLEN] = {};

    if (ht->used == 0) {
        printf("No stats available for empty dictionaries\n");
        return;
    }

    for (i = 0; i < ht->size; ++i) {
        dict_entry *he;
        if (ht->table[i] == NULL) {
            ++clvector[0];
            continue;
        }
        slots++;
        /* For each hash entry on this slot... */
        chainlen = 0;
        he = ht->table[i];
        while (he) {
            ++chainlen;
            he = he->next;
        }
        ++clvector[(chainlen < DICT_STATS_VECTLEN) ? chainlen :
            (DICT_STATS_VECTLEN - 1)];
        if (chainlen > maxchainlen) {
            maxchainlen = chainlen;
        }
        totchainlen += chainlen;
    }
    printf("Hash table stats:\n");
    printf(" table size: %ld\n", ht->size);
    printf(" number of elements: %ld\n", ht->used);
    printf(" different slots: %ld\n", slots);
    printf(" max chain length: %ld\n", maxchainlen);
    printf(" avg chain length (counted): %.02f\n", 
        (float)totchainlen / slots);
    printf(" avg chain length (computed): %.02f\n",
        (float)ht->used / slots);
    printf(" Chain length distribution:\n");
    for (i = 0; i < DICT_STATS_VECTLEN; ++i) {
        if (clvector[i] == 0) {
            continue;
        }
        printf("   %s%ld: %ld (%.02f%%)\n", 
            (i == DICT_STATS_VECTLEN - 1) ? ">= " : "", i, clvector[i],
            ((float)clvector[i] / ht->size) * 100);
    }
}

void dict_print_stats(dict *d) {
    _dict_print_stats_ht(&d->ht[0]);
    if (DICT_IS_REHASHING(d)) {
        printf("-- Rehashing into ht[1]:\n");
        _dict_print_stats_ht(&d->ht[1]);
    }
}

void dict_dump(dict *d) {
    char *key, *val;
    dict_entry *de;
    dict_iterator *iter = dict_get_iterator(d);
    printf("dict contents:\n");
    while ((de = dict_next(iter))) {
        key = DICT_GET_ENTRY_KEY(de);
        val = DICT_GET_ENTRY_VAL(de);
        printf("  %s => %s\n", key, val);
    }
    dict_release_iterator(iter);
}

#ifdef DICT_TEST_MAIN
int randstring(char *target, unsigned int min, unsigned int max) {
    int p = 0;
    int len = min + rand() % (max - min + 1);
    int minval, maxval;
    minval = 'A';
    maxval = 'z';
    while (p < len) {
        target[p++] = minval + rand() % (maxval - minval + 1);
    }
    return len;
}

/* The following is just an example hash table types implementations. */
static unsigned int _demo_hash_func(const void *key) {
    return dict_gen_hash(key, strlen(key));
}

static void *_demo_dup(void *privdata, const void *key) {
    int len = strlen(key);
    char *copy = malloc(len + 1);
    DICT_NOTUSED(privdata);
    memcpy(copy, key, len);
    copy[len] = '\0';
    return copy;
}

static int _demo_cmp(void *privdata, const void *key1, 
        const void *key2) {
    DICT_NOTUSED(privdata);
    return strcmp(key1, key2) == 0;
}

static void _demo_destructor(void *privdata, void *key) {
    DICT_NOTUSED(privdata);
    free(key);
}

dict_type demo_dict = {
    _demo_hash_func,    /* hash function */   
    _demo_dup,          /* key dup */
    _demo_dup,          /* val dup */
    _demo_cmp,          /* key compare */
    _demo_destructor,   /* key destructor */
    _demo_destructor,   /* val destructor */
};

int main(int argc, char *argv[]) {
    char buf[32];
    char buf2[32];
    int i;
    int len;

    srand(time(NULL));
    dict *d = dict_create(&demo_dict, NULL);
    for (i = 0; i < 10000; ++i) {
        len = randstring(buf, 1, sizeof(buf) - 1);
        buf[len] = '\0';
        len = randstring(buf2, 1, sizeof(buf2) - 1);
        buf2[len] = '\0';
        dict_add(d, buf, buf2);
    }
    dict_dump(d);
    dict_empty(d);
    dict_add(d, "foo", "bar");
    dict_add(d, "xxx", "ooo");
    dict_delete(d, "xxx");
    dict_dump(d);

    dict_print_stats(d);
    dict_release(d);
    exit(0);
}
#endif
