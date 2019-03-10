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
#include "long_hash.h"

#include <unistd.h>

#define LOGD(FORMAT, args...)   printf(FORMAT, ##args)
#define LOGI(FORMAT, args...)   printf(FORMAT, ##args)
#define LOGE(FORMAT, args...)   printf(FORMAT, ##args)


/* max 10000 boxes in one obs buffer */
#define OBS_BOX_NUM_MAX  10000
/* max 10000 batchs in one tft buffer */
#define TFT_BATCH_NUM_MAX 10000
/* max 1000 areas in one tft batch */
#define TFT_AREA_NUM_MAX  1000
/* name string should not longer than 128 char */
#define TFT_NAME_LEN_MAX  128


/* TEST marcro */
#define TEST_BUFFER_BATCH_NUM 10
#define TEST_BUFFER_SOURCE_NUM 5
#define TEST_BUFFER_NAME "qj-2288-01"

typedef struct obs_box {
	char *name;
    void *handle;
    /* if==1 ON, if==0 OFF */
    int  status;
	struct obs_box *prev;
	struct obs_box *next;
	unsigned xmin;
	unsigned ymin;
	unsigned xmax;
	unsigned ymax;
}obs_box_t;

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
    obs_box_t *boxes;
	hashTab *boxHash;
}obs_buffer_t;

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
    obs_box_t *box;
	unsigned xmin;
	unsigned ymin;
	unsigned xmax;
	unsigned ymax;
}tft_area_t;

typedef struct tft_batch {
	volatile long ref;
    char *appName;
    char *scenceName;
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
    hashLongTab *batchHash;
}tft_buffer_t;



/* insert new into a bi-direction linker */
void obs_box_insert(obs_box_t *old, obs_box_t *new)
{
    obs_box_t *tmpnext;
    assert(new != NULL);
    if(old == NULL) {
        old->next = old;
        old->prev = old;
        return; 
    }
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

obs_box_t * obs_box_alloc(char *name)
{
    obs_box_t *box = (obs_box_t *)malloc(sizeof(obs_box_t));
    box->name = strdup(name);
    box->status = 0;
    box->prev = box;
    box->next = box;
    box->xmin = 0;
    box->ymin = 0;
    box->xmax = 0;
    box->ymax = 0;
    return box;
}

void obs_batch_insert(obs_batch_t *old, obs_batch_t *new)
{
    obs_batch_t *tmpnext;
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

obs_batch_t * obs_batch_alloc(void)
{
    int i;
    static long ref = 20190302153001;
    obs_batch_t *tmp;
    tmp = (obs_batch_t *)malloc(sizeof(obs_batch_t));
    tmp->ref = ref++;
    tmp->prev = tmp;
    tmp->next = tmp;
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
    buffer->boxes = NULL;
    old = NULL;

    for(i=0; i<TEST_BUFFER_BATCH_NUM; i++)
    {
        new = obs_batch_alloc();
        obs_batch_insert(old, new);
        old = new;
    }
    buffer->batchs = old;
    /* init code boxHash */
    buffer->boxHash = hashInit(OBS_BOX_NUM_MAX);
    
    return buffer;
}

void obs_buffer_free(obs_buffer_t *buffer)
{
    return;
}

void obs_buffer_reset(obs_buffer_t *buf)
{
    obs_box_t *box, *box0;

    if(buf == NULL) return;

    box0 = buf->boxes;
    box = box0;
    do {
        if(box == NULL) return;
        box->status = 0;
        box = box->next;
    }while(box != box0);
}

void obs_boxes_show(obs_buffer_t *buf)
{
    char * name;
    obs_box_t *box, *box0;

    if(buf == NULL) return;

    box0 = buf->boxes;
    box = box0;
    do {
        assert(box != NULL);
        if(box->status == 1) {
            printf("------Box name: %s\n", box->name);
            printf("(xmin,ymin,xmax,ymax): (%d,%d,%d,%d)\n",box->xmin,box->ymin,box->xmax,box->ymax);
        }
        box = box->next;
    }while(box != box0);
}

char * tft_new_name(char *name, int seq)
{
    /* name and max 10 char len of seq number */
    char * new = (char *)malloc(strlen(name)+10);
    assert(name != NULL);
    assert(seq >= 0);
    sprintf(new, "%s%03d", name, seq);
    return new;
}

/* tft area assocate with obs box */
void tft_area_associate(tft_area_t *new)
{
    obs_buffer_t *obsBuffer = new->batch->buffer->obsBuffer;
    hashTab *boxHash;
    obs_box_t *box;
    char * key;

    boxHash = obsBuffer->boxHash;
    assert(boxHash != NULL);
    key = tft_new_name(new->name, new->seq);

    /* check if associate existing box */
    if(new->box != NULL) {
        if(strcmp(key, new->box->name)!=0) {
            LOGE("call tft_area_associate with error params. area name(%s) \
                  should equal box name(%s)\n", key, new->box->name);
            return;
        }
    }

    box = (obs_box_t *)hashGetP(boxHash, key);
    if(box == NULL) {
        /* check if associate existing box */
        if(new->box != NULL) box=new->box;
        else box = obs_box_alloc(key);
        /* insert box into hash table */
        hashSetP(boxHash, key, (void*)box);
        /* insert box into obsBuffer->boxes link */
        if(obsBuffer->boxes == NULL) obsBuffer->boxes = box;
        else obs_box_insert(obsBuffer->boxes, box);   
    }
    /* avoid memory leak */
    free(key);
    /* link box into tft_area_t */
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

tft_area_t * tft_area_alloc(char *name, int seq, enum tft_area_enum type, tft_batch_t *batch, obs_box_t *box)
{
    tft_area_t *tmp;
    tmp = (tft_area_t *)malloc(sizeof(tft_area_t));
    tmp->atype = type;
    tmp->name = strdup(name);
    tmp->seq = seq;
    tmp->batch = batch;
    tmp->prev = tmp;
    tmp->next = tmp;
    tmp->last = NULL;
    tmp->box  = NULL;
    tmp->xmin = 0;
    tmp->ymin = 0;
    tmp->xmax = 0;
    tmp->ymax = 0;

    char *key = tft_new_name(name, seq);

    /* if associate existing box */
    if(box != NULL) {
        if(strcmp(key, box->name)!=0)
            LOGE("call tft_area_associate with error params. area name(%s) \
                  should equal box name(%s)\n", key, box->name);
        else
            tmp->box = box;
    }

    /* avoid mem leak */
    free(key);
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

#if 0
void tft_batch_new(tft_buffer_t *buf, long ref, char *appName, char *scenceName)
{
    tft_batch_t *batch;
    if(buf == NULL || buf->batchHash == NULL || buf->obsBuffer == NULL) {
        LOGE("ERROR: call tft_batch_new with null buf. \
              buf=%p, batchHash=%p, obsBuffer=%p\n",\
              buf, buf->batchHash, buf->obsBuffer);
        return;
    }

    /* check exist batch */
    batch = (tft_batch_t *)hashLongGetP(buf->batchHash, ref);
    if( batch != NULL) {
        /* update app area */
        if (strcmp(appName, batch->appName) != 0) {
            LOGD("tft_batch_new: update %s-%d with new \
                app: %s with old app: %s\n",\
                batch->name, ref, appName, batch->appName);
            assert(hashSetI(batch->areaHash, batch->appName, 1)>=0);
            

    batch = (tft_batch_t *)malloc(sizeof(tft_batch_t));
    batch->prev = batch;
    batch->next = batch;

    batch->buffer = buf;


    /* hash table to avoid name confilt */
    batch->areaHash = hashInit(TFT_AREA_NUM_MAX);
    assert(batch->areaHash != NULL);

    assert(json_typeof(unit) == JSON_OBJECT);

    subunit = json_object_get(unit, "ref");
    assert(json_typeof(subunit) == JSON_INTEGER);
    batch->ref = json_integer_value(subunit);

    subunit = json_object_get(unit, "app");
    assert(json_typeof(subunit) == JSON_STRING);
    batch->appName = strdup(json_string_value(subunit));
    new = tft_area_alloc(batch->appName, 1, TFT_AREA_TYPE_APP, batch, NULL);
    /* avoid area name conflict with reserve app name */
    assert(hashSetI(batch->areaHash, batch->appName, 1)>=0);
    assert(new != NULL);
    tft_area_insert(old, new);
    old = new;

    subunit = json_object_get(unit, "scence");
    assert(json_typeof(subunit) == JSON_STRING);
    batch->scenceName = strdup(json_string_value(subunit));
    new = tft_area_alloc(batch->scenceName, 1, TFT_AREA_TYPE_SCENCE, batch, NULL);
    /* avoid area name conflict with reserve scence name */
    assert(hashSetI(batch->areaHash, batch->scenceName, 1)>=0);
    assert(new != NULL);
    tft_area_insert(old, new);
    old = new;
    
    batch->areas = old;


}

#endif

/* create new subarea in exist tft batch */
void tft_subarea_new(tft_batch_t *batch, char *name, int x1, int x2, int y1, int y2)
{
    int seq;
    tft_area_t *subarea;
    if(batch == NULL || batch->appName == NULL || batch->scenceName == NULL \
        || batch->areaHash == NULL ) {
        LOGE("ERROR: call tft_area_new with null batch. \
              batch=%p, batch.appName=%s, batch.scenceName=%s, areaHash=%p\n",\
              batch, batch->appName, batch->scenceName, batch->areaHash);
        return;
    }

    LOGD("tft_subarea_new in batch ref:%ld\n", batch->ref);

    /* seq areas with same name */
    seq = hashGetI(batch->areaHash, name);
    LOGD("hashGetI(%s)=%d\n",name,seq);
    if(seq <= 0) seq = 1;
    else seq += 1;
    assert(hashSetI(batch->areaHash, name, seq)>=0);
    subarea = tft_area_alloc(name, seq, TFT_AREA_TYPE_SUBAREA, batch, NULL);
    subarea->xmin = x1;
    subarea->ymin = y1;
    subarea->xmax = x2;
    subarea->ymax = y2;
    if(batch->areas == NULL) {
        tft_area_insert(NULL, subarea);
        batch->areas = subarea;
    }
    else tft_area_insert(batch->areas, subarea);
}

/* update exist tft batch */
void tft_subarea_update(tft_area_t *subarea, int x1, int x2, int y1, int y2)
{
    if(subarea == NULL) {
        LOGE("tft_subarea_update called with NULL subarea param\n");
        return;
    }
    subarea->xmin = x1;
    subarea->ymin = y1;
    subarea->xmax = x2;
    subarea->ymax = y2;
}


tft_batch_t * tft_batch_load(json_t *unit, tft_buffer_t *buffer)
{
    int i;
    static long ref = 1;
    tft_batch_t *tmp;
    tft_area_t *old=NULL, *new=NULL;
    json_t *subunit, *aunit, *key;
    char *name;
    int seq;
    int x1,y1,x2,y2;

    tmp = (tft_batch_t *)malloc(sizeof(tft_batch_t));
    tmp->prev = tmp;
    tmp->next = tmp;

    assert(buffer != NULL);
    tmp->buffer = buffer;

    /* hash table to avoid name confilt */
    tmp->areaHash = hashInit(TFT_AREA_NUM_MAX);
    assert(tmp->areaHash != NULL);

    assert(json_typeof(unit) == JSON_OBJECT);

    subunit = json_object_get(unit, "ref");
    assert(json_typeof(subunit) == JSON_INTEGER);
    tmp->ref = json_integer_value(subunit);

    subunit = json_object_get(unit, "app");
    assert(json_typeof(subunit) == JSON_STRING);
    tmp->appName = strdup(json_string_value(subunit));
    new = tft_area_alloc(tmp->appName, 1, TFT_AREA_TYPE_APP, tmp, NULL);
    /* avoid area name conflict with reserve app name */
    assert(hashSetI(tmp->areaHash, tmp->appName, 1)>=0);
    assert(new != NULL);
    tft_area_insert(old, new);
    old = new;

    subunit = json_object_get(unit, "scence");
    assert(json_typeof(subunit) == JSON_STRING);
    tmp->scenceName = strdup(json_string_value(subunit));
    new = tft_area_alloc(tmp->scenceName, 1, TFT_AREA_TYPE_SCENCE, tmp, NULL);
    /* avoid area name conflict with reserve scence name */
    assert(hashSetI(tmp->areaHash, tmp->scenceName, 1)>=0);
    assert(new != NULL);
    tft_area_insert(old, new);
    old = new;
    
    tmp->areas = old;


    subunit = json_object_get(unit, "subareas");
    assert(json_typeof(subunit) == JSON_ARRAY);
    for(i=0; i<json_array_size(subunit); i++)
    {
        aunit = json_array_get(subunit, i);
        assert(json_typeof(aunit) == JSON_OBJECT);
        key = json_object_get(aunit, "type");
        assert(json_typeof(key) == JSON_STRING);
        name = json_string_value(key);

        key = json_object_get(aunit, "xmin");
        assert(json_typeof(key) == JSON_INTEGER);
        x1 = (unsigned)json_integer_value(key);
        key = json_object_get(aunit, "ymin");
        assert(json_typeof(key) == JSON_INTEGER);
        y1 = (unsigned)json_integer_value(key);
        key = json_object_get(aunit, "xmax");
        assert(json_typeof(key) == JSON_INTEGER);
        x2 = (unsigned)json_integer_value(key);
        key = json_object_get(aunit, "ymax");
        assert(json_typeof(key) == JSON_INTEGER);
        y2 = (unsigned)json_integer_value(key);
        
        tft_subarea_new(tmp, name, x1, x2, y1, y2);
    }
    return tmp;
}

void tft_batch_active(tft_batch_t *batch)
{
    tft_area_t *area, *area0;
    obs_box_t *box;

    if(batch == NULL) return;

    area0 = batch->areas;
    area = area0;
    do {
        assert(area != NULL);
        box = area->box;
        assert(box != NULL);

        box->status = 1;
        box->xmin = area->xmin;
        box->ymin = area->ymin;
        box->xmax = area->xmax;
        box->ymax = area->ymax;

        area = area->next;
    }while(area != area0);
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

    /* init batchHash */
    buffer->batchHash = hashLongInit(TFT_BATCH_NUM_MAX);
    
    unit = json_object_get(root, "name");
    assert(json_typeof(unit) == JSON_STRING);
    buffer->name = strdup(json_string_value(unit));

    unit = json_object_get(root, "batchs");
    assert(json_typeof(unit) == JSON_ARRAY);

    old = NULL;
    for(i=0; i<json_array_size(unit); i++)
    {
        new = tft_batch_load(json_array_get(unit, i), buffer);
        assert(new != NULL);
        hashLongSetP(buffer->batchHash, new->ref, (void*)new);
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



void obs_buffer_print(obs_buffer_t *buf)
{
    char * name;
    obs_batch_t *batch, *batch0;
    obs_box_t *box, *box0;
    if(buf == NULL) {
        printf("OBS BUFFER is empty.\n");
        return;
    }

    name = buf->name;
    printf("*************OBS Buffer: %s ***************\n", name);
    batch0 = buf->batchs;
    batch = batch0;
    do {
        assert(batch != NULL);
        printf("------Batch ref: %ld\n", batch->ref);
        printf("xlen: %4d, ylen: %4d\n", batch->xlen, batch->ylen);
        batch = batch->next;
    }while(batch != batch0);

    box0 = buf->boxes;
    box = box0;
    do {
        assert(box != NULL);
        printf("------Box name: %s\n", box->name);
        printf("(xmin,ymin,xmax,ymax): (%d,%d,%d,%d)\n",box->xmin,box->ymin,box->xmax,box->ymax);
        box = box->next;
    }while(box != box0);
}


void tft_buffer_print(tft_buffer_t *buf)
{
    char *name;
    tft_batch_t *batch, *batch0;
    tft_area_t *area, *area0;
    if(buf == NULL) {
        LOGI("TFT BUFFER is empty.\n");
        return;
    }

    name = buf->name;
    printf("*************TFT Buffer: %s ***************\n", name);
    batch0 = buf->batchs;
    batch = batch0;
    do {
        assert(batch != NULL);
        printf("------Batch ref: %ld\n", batch->ref);
        printf("------app.scence: %s.%s\n", batch->appName, batch->scenceName);

        area0 = batch->areas;
        area = area0;
        do {
            assert(area != NULL);
            printf("----area name: type[%d]-%s-seq[%d]\n", area->atype, area->name, area->seq);
            printf("(xmin,ymin,xmax,ymax): (%d,%d,%d,%d)\n",area->xmin,area->ymin,area->xmax,area->ymax);
            area = area->next;
        }while(area != area0);

        batch = batch->next;
    }while(batch != batch0);
}


void test_run(tft_buffer_t *tftBuffer)
{
    obs_buffer_t *obsBuffer;
    obs_batch_t *batch, *batch0;
    obs_box_t *box, *box0;
    hashLongTab *tftBatchHash;
    tft_batch_t *tftBatch;

    obsBuffer = tftBuffer->obsBuffer;
    tftBatchHash = tftBuffer->batchHash;
    if(obsBuffer == NULL) {
        printf("OBS BUFFER is empty.\n");
        return;
    }

    printf("*************Run obs Buffer: %s ***************\n", obsBuffer->name);
    batch0 = obsBuffer->batchs;
    batch = batch0;
    do {
        obs_buffer_reset(obsBuffer);
        assert(batch != NULL);
        printf("------OBS Batch ref: %ld  ", batch->ref);
        printf("xlen: %4d, ylen: %4d\n", batch->xlen, batch->ylen);
        tftBatch = (tft_batch_t *)hashLongGetP(tftBatchHash, batch->ref);
        if(tftBatch == NULL) {
            printf("----NO MATCH TFT\n");
        }
        else {
            tft_batch_active(tftBatch);
            obs_boxes_show(obsBuffer);
        }
        sleep(1);
        batch = batch->next;
    }while(batch != batch0);
}


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
    
    obs_buffer_print(obsBuffer);
    tft_buffer_print(tftBuffer);

    test_run(tftBuffer);
    
    json_decref(root);

    return 0;
}


