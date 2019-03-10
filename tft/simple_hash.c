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
	head->gHashtab = (nlist_t **)malloc(sizeof(nlist_t)*size);
    memset(head->gHashtab, 0 ,sizeof(nlist_t)*size);
	head->gHashSize = size;
	assert(head->gHashtab != NULL);
	return head;
}


/* hash: form hash value for string s */
unsigned eval(char *s, size_t size)
{
    unsigned hindex;
    for (hindex = 0; *s != '\0'; s++)
      hindex = *s + 31 * hindex;
    return hindex % size;
}

/* hashGetP: look the pointer val for Hashtab */
void *hashGetP(hashTab *head, char *s)
{
    struct nlist *np;
    nlist_t **tab = head->gHashtab;
    size_t size = head->gHashSize;
    for (np = tab[eval(s,size)]; np != NULL; np = np->next)
        if (strcmp(s, np->name) == 0)
          return np->pval; /* found */
    return NULL; /* not found */
}

/* hashSetP: put (name, pval) in Hashtab */
int hashSetP(hashTab *head, char *name, void *pval)
{
    struct nlist *np;
    unsigned hindex;
    if ((np = hashGetP(head, name)) == NULL) { /* not found */
        np = (struct nlist *) malloc(sizeof(*np));
        if (np == NULL || (np->name = strdup(name)) == NULL)
          return -ENOMEM;
        hindex = eval(name, head->gHashSize);
        np->next = head->gHashtab[hindex];
        head->gHashtab[hindex] = np;
    } 
	np->pval = pval;
	return 0;
}

/* hashDelK: put (key, pval) in Hashtab */
/*
int hashDelK(hashTab *head, char *key)
{
    struct nlist *np;
    unsigned hindex;
    if ((np = hashGetP(head, key)) == NULL) { 
        return;
    } 
    
	np->pval = pval;
	return 0;
}

*/


/* hashGetI: look the integer val for Hashtab */
int hashGetI(hashTab *head, char *s)
{
    struct nlist *np;
    nlist_t **tab = head->gHashtab;
    size_t size = head->gHashSize;
    for (np = tab[eval(s,size)]; np != NULL; np = np->next)
        if (strcmp(s, np->name) == 0)
          return np->ival; /* found */
    return -ENOENT; /* not found */
}

/* hashSetI: put (name, ival) in Hashtab */
int hashSetI(hashTab *head, char *name, int ival)
{
    struct nlist *np;
    unsigned hindex;
    if ((np = hashGetP(head, name)) == NULL) { /* not found */
        np = (struct nlist *) malloc(sizeof(*np));
        if (np == NULL || (np->name = strdup(name)) == NULL)
          return -ENOMEM;
        hindex = eval(name, head->gHashSize);
        np->next = head->gHashtab[hindex];
        head->gHashtab[hindex] = np;
    } 
	np->ival = ival;
	return 0;
}

/* make a duplicate of s */
/*
char *strdup(char *s) 
{
    char *p;
    p = (char *) malloc(strlen(s)+1);
    if (p != NULL)
       strcpy(p, s);
    return p;
}
*/
