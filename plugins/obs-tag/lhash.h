/******************************************************************************
Utility of a very simple hash table
leo, lili
******************************************************************************/
#ifndef LONG_HASH_H
#define LONG_HASH_H

#include <stdlib.h>

typedef struct hashLongItem { /* table entry: */
    struct hashLongItem *next; /* next entry in chain */
    struct hashLongItem *prev; /* prev entry in chain */
    long key; /* key name */
    void *pval; /* pointer val */
    int   ival; /* integer val*/
}hashLongItem;

/* table entry: */
typedef struct hashLongTab { 
    size_t gHashSize;
    struct hashLongItem **gHashtab;
}hashLongTab;

hashLongTab * hashLongInit(size_t size);
hashLongItem * hashLongGetP(hashLongTab *head, long key);
int hashLongSetP(hashLongTab *head, long key, void *pval);
hashLongItem * hashLongGetI(hashLongTab *head, long key);
int hashLongSetI(hashLongTab *head, long key, int ival);


#endif
