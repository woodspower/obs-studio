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
	head->gHashtab = (hashLongItem **)malloc(sizeof(hashLongItem *)*size);
    memset(head->gHashtab, 0 ,sizeof(hashLongItem*)*size);
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
hashLongItem *hashLongGetP(hashLongTab *head, long key)
{
    hashLongItem *np;
    hashLongItem **tab = head->gHashtab;
    size_t size = head->gHashSize;
    for (np = tab[longEval(key,size)]; np != NULL; np = np->next)
        if (key == np->key)
          return np; /* found */
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
        hindex = longEval(key, head->gHashSize);
        np->next = head->gHashtab[hindex];
        head->gHashtab[hindex] = np;
        np->next = NULL;
        np->prev = NULL;
        hindex = longEval(key, head->gHashSize);
        if(head->gHashtab[hindex] == NULL)
            head->gHashtab[hindex] = np;
        else {
            tmp = head->gHashtab[hindex]->next;
            head->gHashtab[hindex]->next = np;
            np->next = tmp;
            np->prev = head->gHashtab[hindex];
            if ( tmp != NULL)
                tmp->prev = np;
        }
    } 
	np->pval = pval;
	return 0;
}


/* hashLongGetI: look the integer val for Hashtab */
hashLongItem * hashLongGetI(hashLongTab *head, long key)
{
    hashLongItem *np;
    hashLongItem **tab = head->gHashtab;
    size_t size = head->gHashSize;
    for (np = tab[longEval(key,size)]; np != NULL; np = np->next)
        if (key == np->key)
          return np; /* found */
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
        hindex = longEval(key, head->gHashSize);
        if(head->gHashtab[hindex] == NULL)
            head->gHashtab[hindex] = np;
        else {
            tmp = head->gHashtab[hindex]->next;
            head->gHashtab[hindex]->next = np;
            np->next = tmp;
            np->prev = head->gHashtab[hindex];
            if ( tmp != NULL)
                tmp->prev = np;
        }
    } 
	np->ival = ival;
	return 0;
}
