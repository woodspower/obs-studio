/******************************************************************************
Utility of a very simple hash table
leo, lili
******************************************************************************/
#ifndef LONG_HASH_H
#define LONG_HASH_H

#include <stdlib.h>

typedef struct nLongList { /* table entry: */
    struct nLongList *next; /* next entry in chain */
    long key; /* key name */
    void *pval; /* pointer val */
    int   ival; /* integer val*/
}nLongList_t;

/* table entry: */
typedef struct hashLongTab { 
    size_t gHashSize;
    struct nLongList **gHashtab;
}hashLongTab;

hashLongTab * hashLongInit(size_t size);
void *hashLongGetP(hashLongTab *head, long key);
int hashLongSetP(hashLongTab *head, long key, void *pval);
int hashLongGetI(hashLongTab *head, long key);
int hashLongSetI(hashLongTab *head, long key, int ival);


#endif
