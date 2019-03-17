/******************************************************************************
Tensor Flow training lib
leo, lili
******************************************************************************/
#ifndef TFT_H
#define TFT_H

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include "simple_hash.h"
#include "long_hash.h"

/* max 10000 boxes in one obs buffer */
#define TFT_BOX_NUM_MAX  10000
/* max 10000 batchs in one tft buffer */
#define TFT_BATCH_NUM_MAX 10000
/* max 1000 areas in one tft batch */
#define TFT_AREA_NUM_MAX  1000
/* name string should not longer than 128 char */
#define TFT_NAME_LEN_MAX  128



typedef struct tft_box {
	char *name;
    void *handle;
    /* if==1 ON, if==0 OFF */
    int  status;
	struct tft_box *prev;
	struct tft_box *next;
    /* current active tft area */
    struct tft_area *area;
}tft_box_t;

#if 0
typedef struct obs_batch {
	volatile long ref;
	struct obs_batch *prev;
	struct obs_batch *next;
	unsigned int xlen;
	unsigned int ylen;
}obs_batch_t;

typedef struct obs_buffer {
	char *name;
	struct obs_batch *batchs;
}obs_buffer_t;
#endif

enum tft_area_enum {
	TFT_AREA_TYPE_APP,
	TFT_AREA_TYPE_SCENCE,
	TFT_AREA_TYPE_SUBAREA
};


typedef struct tft_area {
	enum tft_area_enum atype;
	char *name;
    /* instance number of same name */
    int seq;
    /* find the last one which have same name */
	struct tft_area *last;
	struct tft_area *prev;
	struct tft_area *next;
    struct tft_batch *batch;
    tft_box_t *box;
	unsigned xmin;
	unsigned ymin;
	unsigned xmax;
	unsigned ymax;
}tft_area_t;

typedef struct tft_batch {
	volatile long ref;
    tft_area_t *appArea;
    tft_area_t *scenceArea;
	struct tft_batch *prev;
	struct tft_batch *next;
    struct tft_buffer *buffer;
	hashTab *areaHash;
	tft_area_t *areas;
}tft_batch_t;

typedef struct tft_buffer {
	char *name;
	struct tft_batch *batchs;
    hashLongTab *batchHash;
    tft_box_t *boxes;
	hashTab *boxHash;
}tft_buffer_t;

#endif
