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
#include "jansson.h"
#include "obs-module.h"
#include "util/shash.h"
#include "util/lhash.h"
#include "util/threading.h"
//#include "graphics/vec2.h"

/* max 10000 boxes in one obs buffer */
#define TFT_BOX_NUM_MAX  10000
/* max 10000 batchs in one tft buffer */
#define TFT_BATCH_NUM_MAX 10000
/* max 1000 areas in one tft batch */
#define TFT_AREA_NUM_MAX  1000
/* name string should not longer than 128 char */
#define TFT_NAME_LEN_MAX  128



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
	char *fullname;
    /* find the last one which have same name */
	struct tft_area *last;
	struct tft_area *prev;
	struct tft_area *next;
    struct tft_batch *batch;
    obs_sceneitem_t  *sceneitem;
	unsigned xmin;
	unsigned ymin;
	unsigned xmax;
	unsigned ymax;
}tft_area_t;

typedef struct tft_batch {
	volatile long ref;
	pthread_mutex_t  mutex;
    tft_area_t *appArea;
    tft_area_t *sceneArea;
	struct tft_batch *prev;
	struct tft_batch *next;
    struct tft_buffer *buffer;
	hashTab *areaHash;
	tft_area_t *areas;
}tft_batch_t;

typedef struct tft_buffer {
	char *name;
    obs_scene_t *scene;
	struct tft_batch *batchs;
    hashLongTab *batchHash;
}tft_buffer_t;

extern tft_buffer_t * tft_buffer_load(obs_scene_t *scene, const char *jsonfile);
extern void tft_batch_active(tft_buffer_t *tftBuffer, long ref);

#endif
