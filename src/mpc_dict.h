
#ifndef __MPC_DICT_H_INCLUDED__
#define __MPC_DICT_H_INCLUDED__


typedef struct dict_entry {
    void *key;
    void *val;
    struct dict_entry *next;
} dict_entry;

typedef struct dict_type {
    unsigned int (*hash_function)(const void *key);
    void *(*key_dup)(void *privdata, const void *key);
    void *(*val_dup)(void *privdata, const void *obj);
    int (*key_cmp)(void *privdata, const void *key1, const void *key2);
    void (*key_destructor)(void *privdata, void *key);
    void (*val_destructor)(void *privdata, void *obj);
} dict_type;

typedef struct dictht {
    dict_entry **table;
    unsigned long size;
    unsigned long sizemask;
    unsigned long used;
} dictht;

typedef struct dict {
    dict_type *type;
    void *privdata;
    dictht ht[2];
    int rehashidx;  /* rehashing not in progress if rehashidx == -1 */
    int iterators;  /* number of iterators currently running */
} dict;

/* If 'safe' is set to 1, this is a safe iterator, that means, you
   can call dict_add, dict_find, and other functions against the 
   dictionary even while iterating. Otherwise it is a non safe 
   iterator, and only dict_next() should be called while iterating. */
typedef struct dict_iterator {
    dict    *d;
    int     table;
    int     index;
    int     safe;
    dict_entry *entry;
    dict_entry *next_entry;
} dict_iterator;

/* This is the initial size of every hash table */
#define DICT_HT_INITIAL_SIZE    4

/* ------------------------ Macros ------------------------*/
#define DICT_SET_HASH_KEY(d, entry, _key_) do { \
    if ((d)->type->key_dup) { \
        entry->key = (d)->type->key_dup((d)->privdata, _key_); \
    } else { \
        entry->key = (_key_); \
    } \
} while (0)

#define DICT_FREE_ENTRY_KEY(d, entry) \
    if ((d)->type->key_destructor) \
        (d)->type->key_destructor((d)->privdata, (entry)->key)

#define DICT_SET_HASH_VAL(d, entry, _val_) do { \
    if ((d)->type->val_dup) { \
        entry->val = (d)->type->val_dup((d)->privdata, _val_); \
    } else { \
        entry->val = (_val_); \
    } \
} while (0)

#define DICT_FREE_ENTRY_VAL(d, entry) \
    if ((d)->type->val_destructor) \
        (d)->type->val_destructor((d)->privdata, (entry)->val)

#define DICT_CMP_HASH_KEYS(d, key1, key2) \
    (((d)->type->key_cmp) ? \
        (d)->type->key_cmp((d)->privdata, key1, key2) : \
        (key1) == (key2))

#define DICT_HASH_KEY(d, key)   (d)->type->hash_function(key)
#define DICT_GET_ENTRY_KEY(de)  ((de)->key)
#define DICT_GET_ENTRY_VAL(de)  ((de)->val)
#define DICT_SLOTS(d)           ((d)->ht[0].size + (d)->ht[1].size)
#define DICT_SIZE(d)            ((d)->ht[0].used + (d)->ht[1].used)
#define DICT_IS_REHASHING(d)    ((d)->rehashidx != -1)

/* API */
dict *dict_create(dict_type *type, void *privdata);
void dict_release(dict *d);
void dict_empty(dict *d);
int dict_add(dict *d, void *key, void *val);
int dict_replace(dict *d, void *key, void *val);
int dict_delete(dict *d, const void *key);
int dict_delete_nofree(dict *d, const void *key);
dict_entry *dict_find(dict *d, const void *key);
void *dict_fetch_value(dict *d, const void *key);
int dict_resize(dict *d);
dict_iterator *dict_get_iterator(dict *d);
dict_iterator *dict_get_safe_iterator(dict *d);
dict_entry *dict_next(dict_iterator *iter);
void dict_release_iterator(dict_iterator *iter);
dict_entry *dict_get_random_key(dict *d);
void dict_print_stats(dict *d);
unsigned int dict_gen_hash(const unsigned char *buf, int len);
unsigned int dict_gen_case_hash(const unsigned char *buf, int len);
void dict_enable_resize(void);
void dict_disable_resize(void);
int dict_rehash(dict *d, int n);
int dict_rehash_milliseconds(dict *d, int ms);


#endif /* __MPC_DICT_H_INCLUDED__ */
