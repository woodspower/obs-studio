/******************************************************************************
Utility of a very simple long hash table
leo, lili
******************************************************************************/
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include "lhash.h"


#define LOGI(FORMAT, args...)   printf(FORMAT, ##args)
#define LOGD(FORMAT, args...)   printf(FORMAT, ##args)
#define LOGE(FORMAT, args...)   printf(FORMAT, ##args)




hashLongTab * hashLongInit(size_t size)
{
    hashLongTab *head;
    head = (hashLongTab *)malloc(sizeof(hashLongTab));
	assert(head != NULL);
	/* pointer table */
	head->gHashtab = (hashLongItem **)malloc(sizeof(hashLongItem *)*size);
    memset(head->gHashtab, 0 ,sizeof(hashLongItem*)*size);
	head->gHashSize = size;
	assert(head->gHashtab != NULL);
	return head;
}

/* insert new into a bi-direction linker */
void hashLongInsert(hashLongItem *old, hashLongItem *new)
{
    hashLongItem *tmpnext;
    assert(new != NULL);
    if(old == NULL)
        return; 
    tmpnext = old->next;
    old->next = new;
    new->prev = old;
    if(tmpnext == old) {
        old->prev = new;
        new->next = old;
    }
    else {
        tmpnext->prev = new;
        new->next = tmpnext;
    }
}


/* hash: form hash value for long key */
unsigned longEval(long key, size_t size)
{
    return key % size;
}

/* hashLongGetP: look the pointer val for Hashtab */
hashLongItem *hashLongGetP(hashLongTab *head, long key)
{
    hashLongItem *np, *np0;
    hashLongItem **tab = head->gHashtab;
    size_t size = head->gHashSize;
    np = tab[longEval(key,size)];
    if (np == NULL) return NULL;
    np0 = np;
    do {
        if (key == np->key)
          return np; /* found */
        np = np->next;
    }while(np != np0);
    return NULL; /* not found */
}

/* hashLongSetP: put (key, pval) in Hashtab */
int hashLongSetP(hashLongTab *head, long key, void *pval)
{
    hashLongItem *np, *tmp;
    unsigned hindex;
    if ((np = hashLongGetP(head, key)) == NULL) { /* not found */
        np = (hashLongItem *) malloc(sizeof(*np));
        if (np == NULL) return -ENOMEM;
        np->key = key;
        np->next = np;
        np->prev = np;
        hindex = longEval(key, head->gHashSize);
        /* insert new into a bi-direction linker */
        if(head->gHashtab[hindex] == NULL)
            head->gHashtab[hindex] = np;
        else hashLongInsert(head->gHashtab[hindex], np);
    } 
	np->pval = pval;
	return 0;
}


/* hashLongGetI: look the integer val for Hashtab */
hashLongItem * hashLongGetI(hashLongTab *head, long key)
{
    hashLongItem *np, *np0;
    hashLongItem **tab = head->gHashtab;
    size_t size = head->gHashSize;
    np = tab[longEval(key,size)];
    if ( np == NULL ) return NULL;
    np0 = np;
    do {
        if (key == np->key)
          return np; /* found */
        np = np->next;
    }while(np != np0);
    return NULL; /* not found */
}

/* hashLongSetI: put (key, ival) in Hashtab */
int hashLongSetI(hashLongTab *head, long key, int ival)
{
    hashLongItem *np, *tmp;
    unsigned hindex;
    if ((np = hashLongGetP(head, key)) == NULL) { /* not found */
        np = (hashLongItem *) malloc(sizeof(*np));
        if (np == NULL) return -ENOMEM;
        np->key = key;
        np->next = np;
        np->prev = np;
        hindex = longEval(key, head->gHashSize);
        /* insert new into a bi-direction linker */
        if(head->gHashtab[hindex] == NULL)
            head->gHashtab[hindex] = np;
        else hashLongInsert(head->gHashtab[hindex], np);
    } 
	np->ival = ival;
	return 0;
}
