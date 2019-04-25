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
	TFT_AREA_TYPE_SCENE,
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
    struct vec2 pos;
    uint32_t width;
    uint32_t height;
}tft_area_t;

typedef struct tft_batch {
	volatile long ref;
    uint32_t width;
    uint32_t height;
	pthread_mutex_t  mutex;
    tft_area_t *appArea;
    tft_area_t *sceneArea;
	struct tft_batch *prev;
	struct tft_batch *next;
    struct tft_buffer *buffer;
	hashTab *areaSeqHash;
	hashTab *areaPtrHash;
	tft_area_t *areas;
}tft_batch_t;

typedef struct tft_buffer {
	char *name;
    obs_scene_t *scene;
    /* current batch ref */
    long curRef;
    /* batch ref need to be set as current */
    long setRef;
	struct tft_batch *batchs;
    hashLongTab *batchHash;
}tft_buffer_t;

extern tft_buffer_t * tft_buffer_load(obs_scene_t *scene, const char *jsonfile);
extern void tft_batch_set(tft_buffer_t *tftBuffer, long ref, uint32_t batchW, uint32_t batchH);
extern void tft_batch_active(tft_buffer_t *tftBuffer);
extern tft_batch_t * tft_batch_update(tft_buffer_t *buf, long ref, char *appName, char *sceneName);
extern void tft_area_update_from_source(tft_buffer_t *tftBuffer, obs_source_t *source);
extern tft_area_t * tft_area_new(tft_batch_t *batch, enum tft_area_enum type,
              const char *name, struct vec2 pos, uint32_t width, uint32_t height);
extern void tft_area_delete(tft_area_t *area);
extern void tft_batch_update_from_obs(tft_buffer_t *tftBuffer);
extern void tft_buffer_print(tft_buffer_t *buf);
extern void tft_exit(tft_buffer_t *buf);

#endif
