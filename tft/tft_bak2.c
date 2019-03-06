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


/* max 10000 areas */
#define TFT_AREA_NUM_MAX  10000
/* name string should not longer than 128 char */
#define TFT_NAME_LEN_MAX  128



typedef struct obs_batch {
	volatile long ref;
	struct obs_batch *next;
	unsigned int xlen;
	unsigned int ylen;
}obs_batch_t;

typedef struct obs_buffer {
	char *name;
	struct obs_batch *batchs;
	hash_t *boxes;
}obs_buffer_t;

enum tft_area_pose_type {
	TF_TAG_POSE_TYPE_1,
	TF_TAG_POSE_TYPE_2
};


typedef struct tft_area {
	char *name;
    void *handle;
	struct tft_area *prev;
	struct tft_area *next;
	enum tft_area_pose_type pose;
	unsigned int xmin;
	unsigned int ymin;
	unsigned int xmax;
	unsigned int ymax;
}tft_area_t;

typedef struct tft_batch {
	volatile long ref;
	struct tft_batch *prev;
	struct tft_batch *next;
	tft_area_t *areas;
}tft_batch_t;

typedef struct tft_buffer {
	char *name;
    obs_buffer_t *obsBuffer;
	struct tft_batch *batchs;
}tft_buffer_t;


#define TEST_BUFFER_BATCH_NUM 100
#define TEST_BUFFER_SOURCE_NUM 5
void test_area_insert(obs_area_t *old, obs_area_t *new)
{
    obs_area_t *pend = old; 
    assert(old != NULL);
    assert(new != NULL);
    while(pend->next != NULL)
        pend = pend->next;
    pend->next = new;
}

obs_area_t * test_area_alloc(void)
{
    static int count = 1;
    obs_area_t *tmp;
    tmp = (obs_area_t *)malloc(sizeof(obs_area_t));
    snprintf(tmp->name, TFT_NAME_LEN_MAX, "obj-%02d",count++);
    tmp->handle = NULL;
    tmp->next = NULL;
    tmp->status = 0;
    tmp->xmin = 10;
    tmp->ymin = 10;
    tmp->xmax = 50;
    tmp->ymax = 50;
    return tmp;
}

void test_batch_insert(obs_batch_t *old, obs_batch_t *new)
{
    obs_batch_t *pend = old; 
    assert(old != NULL);
    assert(new != NULL);
    while(pend->next != NULL)
        pend = pend->next;
    pend->next = new;
}

obs_batch_t * test_batch_alloc(void)
{
    int i;
    static long ref = 1;
    obs_batch_t *tmp;
    obs_area_t *old, *new;
    tmp = (obs_batch_t *)malloc(sizeof(obs_batch_t));
    tmp->ref = ref++;
    tmp->next = NULL;
    tmp->xlen = 500;
    tmp->ylen = 500;
    return tmp;
}

obs_buffer_t * test_buffer_load(void)
{
    int i;
    struct obs_buffer *buffer;
    obs_batch_t *old, *new;
    buffer = (obs_buffer_t*)malloc(sizeof(obs_buffer_t));
    assert(buffer != NULL);
    snprintf(buffer->name, TFT_NAME_LEN_MAX, "MyTestBuffer1");
    buffer->batchs = test_batch_alloc();
    old = buffer->batchs;
    for(i=0; i<TEST_BUFFER_BATCH_NUM; i++)
    {
        new = test_batch_alloc();
        test_batch_insert(old, new);
        old = new;
    }
    /* init text areas */
    buffer->boxes = hashInit(TFT_AREA_NUM_MAX);
    
    return buffer;
}

void test_buffer_free(obs_buffer_t *buffer)
{
    return;
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

tft_area_t * tft_area_alloc(void)
{
    static int count = 1;
    tft_area_t *tmp;
    tmp = (tft_area_t *)malloc(sizeof(tft_area_t));
    snprintf(tmp->name, TFT_NAME_LEN_MAX, "obj-%02d",count++);
    tmp->handle = NULL;
    tmp->prev = NULL;
    tmp->next = NULL;
    tmp->pose = TF_TAG_POSE_TYPE_1;
    tmp->xmin = 10;
    tmp->ymin = 10;
    tmp->xmax = 50;
    tmp->ymax = 50;
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

tft_batch_t * tft_batch_alloc(json_t *unit)
{
    int i;
    static long ref = 1;
    tft_batch_t *tmp;
    tft_area_t *old, *new;
    json_t *subunit;

    tmp = (tft_batch_t *)malloc(sizeof(tft_batch_t));
    tmp->prev = NULL;
    tmp->next = NULL;

    assert(json_typeof(unit) == JSON_OBJECT);

    subunit = json_object_get(unit, "ref");
    assert(json_typeof(subunit) == JSON_INTEGER);
    tmp->ref = json_integer_value(subunit);

    subunit = json_object_get(unit, "app");
    assert(json_typeof(subunit) == JSON_STRING);
    tmp->name = strdup(json_string_value(subunit));

    old = NULL;
    for(i=0; i<TEST_BUFFER_SOURCE_NUM; i++)
    {
        new = tft_area_alloc();
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
    snprintf(buffer->name, TFT_NAME_LEN_MAX, "%s", json_string_value(unit));

    unit = json_object_get(root, "batchs");
    assert(json_typeof(unit) == JSON_ARRAY);

    old = NULL;
    for(i=0; i<json_array_size(unit); i++)
    {
        new = tft_batch_alloc(json_array_get(unit, i));
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
    obs_buffer_t *buffer;
    tft_buffer_t *tftBuffer;
    json_t *root;
    json_error_t error;

    if (argc != 1) {
        fprintf(stderr, "Usage: %s\n", argv[0]);
        exit(-1);
    }

    /* init a demo buffer */
    buffer = test_buffer_load();

    /* load ai result from json file */
    root = json_load_file(TEST_AI_CONFIG_JSON, 0, &error);
    if(root == NULL) {
        fprintf(stderr, "Error MSG: %s\n", error.text);
        exit(-1);
    }

    tftBuffer = tft_buffer_load(root, buffer);
    
    
    json_decref(root);

    return 0;
}


