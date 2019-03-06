/******************************************************************************
Utility of tag data process for tfrecord generation.
leo, lili
******************************************************************************/

/*
#include "util/bmem.h"
#include "util/threading.h"
#include "util/dstr.h"
#include "util/darray.h"
#include "util/platform.h"
#include "graphics/vec2.h"
#include "graphics/vec3.h"
#include "graphics/vec4.h"
#include "graphics/quat.h"
#include "obs-data.h"
*/

#include <string.h>
#include <jansson.h>
#include <assert.h>
#include "simple_hash.h"


#define LOGI(FORMAT, args...)   printf(FORMAT, ##args)
#define LOGD(FORMAT, args...)   printf(FORMAT, ##args)
#define LOGE(FORMAT, args...)   printf(FORMAT, ##args)


/* max 10000 boxes in one obs buffer */
#define OBS_BOX_NUM_MAX  10000
/* max 1000 areas in one tft batch */
#define TFT_AREA_NUM_MAX  1000
/* name string should not longer than 128 char */
#define TFT_NAME_LEN_MAX  128


typedef struct obs_box {
	char *name;
    int  seq;
    void *handle;
	unsigned xmin;
	unsigned ymin;
	unsigned xmax;
	unsigned ymax;
}obs_box_t;

typedef struct obs_batch {
	volatile long ref;
	struct obs_batch *next;
	unsigned int xlen;
	unsigned int ylen;
}obs_batch_t;

typedef struct obs_buffer {
	char *name;
	struct obs_batch *batchs;
	hashTab *boxes;
}obs_buffer_t;

enum tft_area_enum {
	TFT_AREA_TYPE_APP,
	TFT_AREA_TYPE_SCENCE,
	TFT_AREA_TYPE_AREA
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
    obs_box_t *box;
	unsigned xmin;
	unsigned ymin;
	unsigned xmax;
	unsigned ymax;
}tft_area_t;

typedef struct tft_batch {
	volatile long ref;
    char *appType;
    char *scenceType;
	struct tft_batch *prev;
	struct tft_batch *next;
    struct tft_buffer *buffer;
	hashTab *areaHash;
	tft_area_t *areas;
}tft_batch_t;

typedef struct tft_buffer {
	char *name;
    obs_buffer_t *obsBuffer;
	struct tft_batch *batchs;
}tft_buffer_t;


#define TEST_BUFFER_BATCH_NUM 100
#define TEST_BUFFER_SOURCE_NUM 5
#define TEST_BUFFER_NAME "qj-2288-01"


obs_box_t * obs_box_alloc(char *name, int seq)
{
    obs_box_t *box = (obs_box_t *)malloc(sizeof(obs_box_t));
    box->name = strdup(name);
    box->seq =seq;
    box->xmin = 0;
    box->ymin = 0;
    box->xmax = 0;
    box->ymax = 0;
    return box;
}

void obs_batch_insert(obs_batch_t *old, obs_batch_t *new)
{
    obs_batch_t *pend = old; 
    assert(old != NULL);
    assert(new != NULL);
    while(pend->next != NULL)
        pend = pend->next;
    pend->next = new;
}

obs_batch_t * obs_batch_alloc(void)
{
    int i;
    static long ref = 1;
    obs_batch_t *tmp;
    tmp = (obs_batch_t *)malloc(sizeof(obs_batch_t));
    tmp->ref = ref++;
    tmp->next = NULL;
    tmp->xlen = 500;
    tmp->ylen = 500;
    return tmp;
}

obs_buffer_t * obs_buffer_load(void)
{
    int i;
    struct obs_buffer *buffer;
    obs_batch_t *old, *new;
    buffer = (obs_buffer_t*)malloc(sizeof(obs_buffer_t));
    assert(buffer != NULL);
    buffer->name = strdup(TEST_BUFFER_NAME);
    buffer->batchs = obs_batch_alloc();
    old = buffer->batchs;
    for(i=0; i<TEST_BUFFER_BATCH_NUM; i++)
    {
        new = obs_batch_alloc();
        obs_batch_insert(old, new);
        old = new;
    }
    /* init code boxes */
    buffer->boxes = hashInit(OBS_BOX_NUM_MAX);
    
    return buffer;
}

void obs_buffer_free(obs_buffer_t *buffer)
{
    return;
}

/* tft area assocate with obs box */
void tft_area_associate(tft_area_t *new)
{
    hashTab *boxes = new->batch->buffer->obsBuffer->boxes;
    obs_box_t *box;
    /* name and max 10 char len of seq number */
    char * key = (char *)malloc(strlen(new->name)+10);

    assert(boxes != NULL);
    assert(new->seq >= 0);
    sprintf(key, "%s%03d", new->name, new->seq);

    box = (obs_box_t *)hashGetP(boxes, key);
    if(box == NULL) {
        box = obs_box_alloc(new->name, new->seq);
        hashSetP(boxes, key, (void*)box);
    }
    new->box = box;
}


void tft_area_insert(tft_area_t *old, tft_area_t *new)
{
    tft_area_t *tmpnext;
    assert(new != NULL);
    if(old == NULL)
       return; 
    tmpnext = old->next;
    old->next = new;
    new->next = tmpnext;
    new->prev = old;
    tmpnext->prev = new;
}

tft_area_t * tft_area_alloc(char *name, int seq, enum tft_area_enum type, tft_batch_t *batch)
{
    tft_area_t *tmp;
    tmp = (tft_area_t *)malloc(sizeof(tft_area_t));
    tmp->atype = type;
    tmp->name = strdup(name);
    tmp->seq = seq;
    tmp->batch = batch;
    tmp->prev = NULL;
    tmp->next = NULL;
    tmp->last = NULL;
    tmp->box = NULL;
    tmp->batch = NULL;
    tmp->xmin = 0;
    tmp->ymin = 0;
    tmp->xmax = 0;
    tmp->ymax = 0;

    /* associate tft area with obs box */
    tft_area_associate(tmp);

    return tmp;
}

void tft_batch_insert(tft_batch_t *old, tft_batch_t *new)
{
    tft_batch_t *tmpnext;
    assert(new != NULL);
    if(old == NULL)
       return; 
    tmpnext = old->next;
    old->next = new;
    new->next = tmpnext;
    new->prev = old;
    tmpnext->prev = new;
}


tft_batch_t * tft_batch_alloc(json_t *unit, tft_buffer_t *buffer)
{
    int i;
    static long ref = 1;
    tft_batch_t *tmp;
    tft_area_t *old, *new;
    json_t *subunit, *aunit, *key;
    char *name;
    int seq;

    tmp = (tft_batch_t *)malloc(sizeof(tft_batch_t));
    tmp->prev = NULL;
    tmp->next = NULL;

    assert(buffer != NULL);
    tmp->buffer = buffer;

    assert(json_typeof(unit) == JSON_OBJECT);

    subunit = json_object_get(unit, "ref");
    assert(json_typeof(subunit) == JSON_INTEGER);
    tmp->ref = json_integer_value(subunit);

    subunit = json_object_get(unit, "app");
    assert(json_typeof(subunit) == JSON_STRING);
    tmp->appType = strdup(json_string_value(subunit));
    old = tft_area_alloc(tmp->appType, 1, TFT_AREA_TYPE_APP, tmp);
    assert(old != NULL);

    subunit = json_object_get(unit, "scence");
    assert(json_typeof(subunit) == JSON_STRING);
    tmp->scenceType = strdup(json_string_value(subunit));
    new = tft_area_alloc(tmp->scenceType, 1, TFT_AREA_TYPE_SCENCE, tmp);
    assert(new != NULL);
    tft_area_insert(old, new);
    old = new;

    tmp->areaHash = hashInit(TFT_AREA_NUM_MAX);
    assert(tmp->areaHash != NULL);
    subunit = json_object_get(unit, "areas");
    assert(json_typeof(subunit) == JSON_ARRAY);
    for(i=0; i<json_array_size(subunit); i++)
    {
        aunit = json_array_get(subunit, i);
        assert(json_typeof(aunit) == JSON_OBJECT);
        key = json_object_get(aunit, "type");
        assert(json_typeof(key) == JSON_STRING);
        name = json_string_value(key);
        /* seq areas with same name */
        seq = hashGetI(tmp->areaHash, name);
        if(seq <= 0) seq = 1;
        else seq += 1;
        assert(hashSetI(tmp->areaHash, name, seq)>=0);
        new = tft_area_alloc(name, seq, TFT_AREA_TYPE_AREA, tmp);
        key = json_object_get(aunit, "xmin");
        assert(json_typeof(key) == JSON_INTEGER);
        new->xmin = (unsigned)json_integer_value(key);
        key = json_object_get(aunit, "ymin");
        assert(json_typeof(key) == JSON_INTEGER);
        new->ymin = (unsigned)json_integer_value(key);
        key = json_object_get(aunit, "xmax");
        assert(json_typeof(key) == JSON_INTEGER);
        new->xmax = (unsigned)json_integer_value(key);
        key = json_object_get(aunit, "ymax");
        assert(json_typeof(key) == JSON_INTEGER);
        new->ymax = (unsigned)json_integer_value(key);

        tft_area_insert(old, new);
        old = new;
    }
    tmp->areas = old;
    return tmp;
}

tft_buffer_t * tft_buffer_load(json_t *root, obs_buffer_t *obsBuffer)
{
    int i;
    struct tft_buffer *buffer;
    tft_batch_t *old, *new;
    const char *key;
    json_t *unit, *subunit;
    char *name;

    assert(root != NULL);
    assert(obsBuffer != NULL);

    assert(json_typeof(root) == JSON_OBJECT);
    buffer = (tft_buffer_t*)malloc(sizeof(tft_buffer_t));
    assert(buffer != NULL);
    buffer->obsBuffer = obsBuffer;

    
    unit = json_object_get(root, "name");
    assert(json_typeof(unit) == JSON_STRING);
    buffer->name = strdup(json_string_value(unit));

    unit = json_object_get(root, "batchs");
    assert(json_typeof(unit) == JSON_ARRAY);

    old = NULL;
    for(i=0; i<json_array_size(unit); i++)
    {
        new = tft_batch_alloc(json_array_get(unit, i), buffer);
        tft_batch_insert(old, new);
        old = new;
    }
    buffer->batchs = old;
    return buffer;
}

void tft_buffer_free(obs_buffer_t *buffer)
{
    return;
}

/*
tf_tag_batch * tf_tag_load(json_t *root)
    switch (json_typeof(element)) {
    case JSON_OBJECT:
        print_json_object(element, indent);
    size = json_object_size(element);

    printf("JSON Object of %ld pair%s:\n", size, json_plural(size));
    json_object_foreach(element, key, value) {
        print_json_indent(indent + 2);
        printf("JSON Key: \"%s\"\n", key);
        print_json_aux(value, indent + 2);
    }
*/
#define TEST_AI_CONFIG_JSON "./main.json"


int main(int argc, char *argv[]) {
    obs_buffer_t *obsBuffer;
    tft_buffer_t *tftBuffer;
    json_t *root;
    json_error_t error;

    if (argc != 1) {
        fprintf(stderr, "Usage: %s\n", argv[0]);
        exit(-1);
    }

    /* init a demo buffer */
    obsBuffer = obs_buffer_load();

    /* load ai result from json file */
    root = json_load_file(TEST_AI_CONFIG_JSON, 0, &error);
    if(root == NULL) {
        fprintf(stderr, "Error MSG: %s\n", error.text);
        exit(-1);
    }

    tftBuffer = tft_buffer_load(root, obsBuffer);
    
    
    json_decref(root);

    return 0;
}


