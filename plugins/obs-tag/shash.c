/******************************************************************************
Utility of a very simple hash table
leo, lili
******************************************************************************/
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include "shash.h"


#define LOGI(FORMAT, args...)   printf(FORMAT, ##args)
#define LOGD(FORMAT, args...)   printf(FORMAT, ##args)
#define LOGE(FORMAT, args...)   printf(FORMAT, ##args)




hashTab * hashInit(size_t size)
{
    hashTab *head;
    head = (hashTab *)malloc(sizeof(hashTab));
	assert(head != NULL);
	/* pointer table */
	head->gHashtab = (hashItem **)malloc(sizeof(hashItem *)*size);
    memset(head->gHashtab, 0 ,sizeof(hashItem *)*size);
	head->gHashSize = size;
	assert(head->gHashtab != NULL);
	return head;
}


/* insert new into a bi-direction linker */
void hashInsert(hashItem *old, hashItem *new)
{
    hashItem *tmpnext;
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

/* hash: form hash value for string key */
unsigned eval(char *key, size_t size)
{
    unsigned hindex;
    for (hindex = 0; *key != '\0'; key++)
      hindex = *key + 31 * hindex;
    return hindex % size;
}

/* hashGetP: look the pointer val for Hashtab */
hashItem *hashGetP(hashTab *head, char *key)
{
    hashItem *np, *np0;
    hashItem **tab = head->gHashtab;
    size_t size = head->gHashSize;
    np = tab[eval(key,size)];
    if (np == NULL) return NULL;
    np0 = np;
    do {
        if (strcmp(key, np->key) == 0)
          return np; /* found */
        np = np->next;
    }while(np != np0);
    return NULL; /* not found */
}

/* hashSetP: put (key, pval) in Hashtab */
int hashSetP(hashTab *head, char *key, void *pval)
{
    hashItem *np, *tmp;
    unsigned hindex;
    if ((np = hashGetP(head, key)) == NULL) { /* not found */
        np = (hashItem *) malloc(sizeof(*np));
        if (np == NULL || (np->key = strdup(key)) == NULL)
          return -ENOMEM;
        np->next = np;
        np->prev = np;
        hindex = eval(key, head->gHashSize);
        /* insert new into a bi-direction linker */
        if(head->gHashtab[hindex] == NULL)
            head->gHashtab[hindex] = np;
        else hashInsert(head->gHashtab[hindex], np);
    } 
	np->pval = pval;
	return 0;
}

/* hashDelK: put (key, pval) in Hashtab */
void hashDelK(hashTab *head, char *key)
{
    hashItem *np, *tmpNext, *tmpPrev;
    unsigned hindex;
    np = hashGetP(head, key);
    if (np == NULL)
        return;

    hindex = eval(key, head->gHashSize);
    tmpPrev = np->prev;
    tmpNext = np->next;

    /* if it is the last item */
    if(tmpNext == np) 
        head->gHashtab[hindex] = NULL;
    else {
        tmpPrev->next = tmpNext;
        tmpNext->prev = tmpPrev;
    }

    /* if it is the head->gHashtab[] itself */
    if(head->gHashtab[hindex] == np)
        head->gHashtab[hindex] = tmpNext;
        
    /* remove np link info */
    np->next = NULL;
    np->prev = NULL;

    free(np);
}



/* hashGetI: look the integer val for Hashtab */
hashItem * hashGetI(hashTab *head, char *key)
{
    hashItem *np, *np0;
    hashItem **tab = head->gHashtab;
    size_t size = head->gHashSize;
    np = tab[eval(key,size)];
    if (np == NULL) return NULL;
    np0 = np;
    do {
        if (strcmp(key, np->key) == 0)
          return np; /* found */
        np = np->next;
    }while(np != np0);
    return NULL; /* not found */
}

/* hashSetI: put (key, ival) in Hashtab */
int hashSetI(hashTab *head, char *key, int ival)
{
    hashItem *np, *tmp;
    unsigned hindex;
    if ((np = hashGetP(head, key)) == NULL) { /* not found */
        np = (hashItem *) malloc(sizeof(*np));
        if (np == NULL || (np->key = strdup(key)) == NULL)
          return -ENOMEM;
        np->next = np;
        np->prev = np;
        hindex = eval(key, head->gHashSize);
        /* insert new into a bi-direction linker */
        if(head->gHashtab[hindex] == NULL)
            head->gHashtab[hindex] = np;
        else hashInsert(head->gHashtab[hindex], np);
    } 
	np->ival = ival;
	return 0;
}

/* make a duplicate of key */
/*
char *strdup(char *key) 
{
    char *p;
    p = (char *) malloc(strlen(key)+1);
    if (p != NULL)
       strcpy(p, key);
    return p;
}
*/
