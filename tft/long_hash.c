/******************************************************************************
Utility of a very simple long hash table
leo, lili
******************************************************************************/
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include "long_hash.h"


#define LOGI(FORMAT, args...)   printf(FORMAT, ##args)
#define LOGD(FORMAT, args...)   printf(FORMAT, ##args)
#define LOGE(FORMAT, args...)   printf(FORMAT, ##args)




hashLongTab * hashLongInit(size_t size)
{
    hashLongTab *head;
    head = (hashLongTab *)malloc(sizeof(hashLongTab));
	assert(head != NULL);
	/* pointer table */
	head->gHashtab = (nLongList_t **)malloc(sizeof(nLongList_t)*size);
	head->gHashSize = size;
	assert(head->gHashtab != NULL);
	return head;
}


/* hash: form hash value for long key */
unsigned longEval(long key, size_t size)
{
    return key % size;
}

/* hashLongGetP: look the pointer val for Hashtab */
void *hashLongGetP(hashLongTab *head, long key)
{
    struct nLongList *np;
    nLongList_t **tab = head->gHashtab;
    size_t size = head->gHashSize;
    for (np = tab[longEval(key,size)]; np != NULL; np = np->next)
        if (key == np->key)
          return np->pval; /* found */
    return NULL; /* not found */
}

/* hashLongSetP: put (key, pval) in Hashtab */
int hashLongSetP(hashLongTab *head, long key, void *pval)
{
    struct nLongList *np;
    unsigned hindex;
    if ((np = hashLongGetP(head, key)) == NULL) { /* not found */
        np = (struct nLongList *) malloc(sizeof(*np));
        if (np == NULL) return -ENOMEM;
        np->key = key;
        hindex = longEval(key, head->gHashSize);
        np->next = head->gHashtab[hindex];
        head->gHashtab[hindex] = np;
    } 
	np->pval = pval;
	return 0;
}

/* hashLongGetI: look the integer val for Hashtab */
int hashLongGetI(hashLongTab *head, long key)
{
    struct nLongList *np;
    nLongList_t **tab = head->gHashtab;
    size_t size = head->gHashSize;
    for (np = tab[longEval(key,size)]; np != NULL; np = np->next)
        if (key == np->key)
          return np->ival; /* found */
    return -ENOENT; /* not found */
}

/* hashLongSetI: put (key, ival) in Hashtab */
int hashLongSetI(hashLongTab *head, long key, int ival)
{
    struct nLongList *np;
    unsigned hindex;
    if ((np = hashLongGetP(head, key)) == NULL) { /* not found */
        np = (struct nLongList *) malloc(sizeof(*np));
        if (np == NULL) return -ENOMEM;
        np->key = key;
        hindex = longEval(key, head->gHashSize);
        np->next = head->gHashtab[hindex];
        head->gHashtab[hindex] = np;
    } 
	np->ival = ival;
	return 0;
}

