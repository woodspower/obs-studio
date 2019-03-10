/******************************************************************************
Utility of a very simple hash table
leo, lili
******************************************************************************/
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include "simple_hash.h"


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
    hashItem *np;
    hashItem **tab = head->gHashtab;
    size_t size = head->gHashSize;
    for (np = tab[eval(key,size)]; np != NULL; np = np->next)
        if (strcmp(key, np->key) == 0)
          return np; /* found */
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
        np->next = NULL;
        np->prev = NULL;
        hindex = eval(key, head->gHashSize);
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

/* hashDelK: put (key, pval) in Hashtab */
/*
int hashDelK(hashTab *head, char *key)
{
    hashItem *np;
    unsigned hindex;
    if ((np = hashGetP(head, key)) == NULL) { 
        return;
    } 
    
	np->pval = pval;
	return 0;
}

*/


/* hashGetI: look the integer val for Hashtab */
hashItem * hashGetI(hashTab *head, char *key)
{
    hashItem *np;
    hashItem **tab = head->gHashtab;
    size_t size = head->gHashSize;
    for (np = tab[eval(key,size)]; np != NULL; np = np->next)
        if (strcmp(key, np->key) == 0)
          return np; /* found */
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
        hindex = eval(key, head->gHashSize);
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
