/******************************************************************************
Utility of a very simple hash table
leo, lili
******************************************************************************/
#ifndef SIMPLE_HASH_H
#define SIMPLE_HASH_H

#include <stdlib.h>

typedef struct nlist { /* table entry: */
    struct nlist *next; /* next entry in chain */
    char *name; /* key name */
    void *pval; /* pointer val */
    int   ival; /* integer val*/
}nlist_t;

/* table entry: */
typedef struct hashTab { 
    size_t gHashSize;
    struct nlist **gHashtab;
}hashTab;

hashTab * hashInit(size_t size);
void *hashGetP(hashTab *head, char *s);
int hashSetP(hashTab *head, char *name, void *pval);
int hashGetI(hashTab *head, char *s);
int hashSetI(hashTab *head, char *name, int ival);


#endif
