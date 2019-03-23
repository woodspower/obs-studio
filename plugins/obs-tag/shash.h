/******************************************************************************
Utility of a very simple hash table
leo, lili
******************************************************************************/
#ifndef STRING_HASH_H
#define STRING_HASH_H

#include <stdlib.h>

typedef struct hashItem { /* table entry: */
    struct hashItem *next; /* next entry in chain */
    struct hashItem *prev; /* prev entry in chain */
    char *key; /* key key */
    void *pval; /* pointer val */
    int   ival; /* integer val*/
}hashItem;

/* table entry: */
typedef struct hashTab { 
    size_t gHashSize;
    hashItem **gHashtab;
}hashTab;

hashTab * hashInit(size_t size);
hashItem * hashGetP(hashTab *head, char *s);
int hashSetP(hashTab *head, char *key, void *pval);
hashItem * hashGetI(hashTab *head, char *s);
int hashSetI(hashTab *head, char *key, int ival);
void hashDelK(hashTab *head, char *key);


#endif
