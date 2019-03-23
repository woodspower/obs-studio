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

#include "tft.h"


#define LOGD(FORMAT, args...)   printf(FORMAT, ##args)
#define LOGI(FORMAT, args...)   printf(FORMAT, ##args)
#define LOGE(FORMAT, args...)   printf(FORMAT, ##args)

/* TEST marcro */
#define TEST_BUFFER_BATCH_NUM 10
#define TEST_BUFFER_SOURCE_NUM 5
#define TEST_BUFFER_NAME "qj-2288-01"


/* insert new into a bi-direction linker */
void tft_box_insert(tft_box_t *old, tft_box_t *new)
{
    tft_box_t *tmpnext;
    assert(new != NULL);
    if(old == NULL) {
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

#if 0
static void tft_box_add_scene(obs_scene_t *scene, obs_source_t *source)
{
	obs_sceneitem_t *item = NULL;
	struct vec2 scale;

	//vec2_set(&scale, 20.0f, 20.0f);

	item = obs_scene_add(scene, source);
	//obs_sceneitem_set_scale(item, &scale);
}

obs_source_t * obs_source_create(char *type, char *name, char *setting, char *key)
{
    return (obs_source_t *)malloc(sizeof(obs_source_t));
}
#endif

tft_box_t * tft_box_alloc(obs_scene_t *scene, char *name)
{
	obs_source_t *source;
    tft_box_t *box = (tft_box_t *)malloc(sizeof(tft_box_t));
    if (box == NULL) {
        LOGE("tft_box_alloc out of memory.\n");
        return NULL;
    }
   
	source = obs_source_create("obs_tag", name, NULL, NULL);
    if (source == NULL) {
        LOGE("tft_box_alloc source create failed.\n");
        free(box);
        return NULL;
    }

    box->source = source;
    box->sceneitem = obs_scene_add(scene, source);
    box->name = strdup(name);
    box->status = 0;
    box->prev = box;
    box->next = box;
    box->area = NULL;
    return box;
}

#if 0
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
    old = NULL;

    for(i=0; i<TEST_BUFFER_BATCH_NUM; i++)
    {
        new = obs_batch_alloc();
        obs_batch_insert(old, new);
        old = new;
    }
    buffer->batchs = old;
    
    return buffer;
}

void obs_buffer_free(obs_buffer_t *buffer)
{
    return;
}

#endif

void tft_box_reset(tft_buffer_t *buf)
{
    tft_box_t *box, *box0;

    if(buf == NULL) return;

    box0 = buf->boxes;
    box = box0;
    do {
        if(box == NULL) return;
        box->status = 0;
        if (box->sceneitem != NULL)
            obs_sceneitem_set_visible(box->sceneitem, false);
        box = box->next;
    }while(box != box0);
}

void tft_box_show(tft_buffer_t *buf)
{
    tft_box_t *box, *box0;
    tft_area_t *area;

    if(buf == NULL) return;

    box0 = buf->boxes;
    box = box0;
    do {
        if(box == NULL) {
            LOGD("tft_box_show: NO box now.\n");
            return;
        }
        if(box->status == 1) {
            LOGI("------Box name: %s\n", box->name);
            area = box->area;
            assert (area != NULL);
            LOGI("type:%d (xmin,ymin,xmax,ymax): (%d,%d,%d,%d)\n",
            area->atype, area->xmin,area->ymin,area->xmax,area->ymax);
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
    tft_buffer_t *tftBuffer = new->batch->buffer;
    hashTab *boxHash;
    hashItem *item;
    tft_box_t *box;
    char * key;

    if(tftBuffer == NULL) {
        LOGE("tft_area_associate called but tftBuffer is NULL.\n");
        return;
    }

    if(tftBuffer->scene == NULL) {
        LOGE("tft_area_associate called but tftBuffer->scene is NULL.\n");
        return;
    }

    boxHash = tftBuffer->boxHash;
    assert(boxHash != NULL);
    key = tft_new_name(new->name, new->seq);

    /* check if associate existing box */
    if(new->box != NULL) {
        if(strcmp(key, new->box->name)!=0) {
            LOGE("call tft_area_associate with error params. area name(%s) \
                  should equal box name(%s)\n", key, new->box->name);
        }
        /* do not need create new box */
        return;
    }

    item = hashGetP(boxHash, key);
    if (item != NULL) {
        assert(item->pval != NULL);
        box = (tft_box_t *)item->pval;
    }
    else {
        /* check if associate existing box */
        if(new->box != NULL) box=new->box;
        else box = tft_box_alloc(tftBuffer->scene, key);
        /* insert box into hash table */
        hashSetP(boxHash, key, (void*)box);
        /* insert box into tftBuffer->boxes link */
        if(tftBuffer->boxes == NULL) tftBuffer->boxes = box;
        else tft_box_insert(tftBuffer->boxes, box);   
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

void tft_area_free(tft_area_t *item)
{
    tft_batch_t *batch;
    tft_area_t *tmpNext, *tmpPrev;

    if(item == NULL) {
        LOGE("tft_area_free called with NULL item\n");
        return;
    }
    batch = item->batch;
    tmpNext = item->next;
    tmpPrev = item->prev;

    /* if it is the last item */
    if(tmpNext == item)
        batch->areas = NULL;
    else {
        tmpPrev->next = tmpNext;
        tmpNext->prev = tmpPrev;
    }

    /* if it is the batch->areas itself */
    if(batch->areas == item)
        batch->areas = tmpNext;
        
    /* remove item link info */
    item->next = NULL;
    item->prev = NULL;

    /* do not need to call disassociate and direct free */
    // tft_area_disassociate(item);
    free(item);
}

tft_area_t * tft_area_alloc(tft_batch_t *batch, char *name, enum tft_area_enum type, int x1, int y1, int x2, int y2)
{
    tft_area_t *tmp;
    hashItem *item;

    tmp = (tft_area_t *)malloc(sizeof(tft_area_t));
    tmp->atype = type;
    tmp->name = strdup(name);
    tmp->batch = batch;
    tmp->prev = tmp;
    tmp->next = tmp;
    tmp->last = NULL;
    tmp->box  = NULL;
    tmp->xmin = x1;
    tmp->ymin = y1;
    tmp->xmax = x2;
    tmp->ymax = y2;

    /* add counter hash for same name */
    item = hashGetI(batch->areaHash, name);
    if (item != NULL) tmp->seq = item->ival + 1;
    else tmp->seq = 1;
    LOGD("hashGetI(%s)=%d\n",name,tmp->seq);
    assert(hashSetI(batch->areaHash, name, tmp->seq)>=0);

    /* add into batch->areas linker */
    if(batch->areas == NULL) {
        tft_area_insert(NULL, tmp);
        batch->areas = tmp;
    }
    else tft_area_insert(batch->areas, tmp);

    /* pre-associate tft area with obs box */
    // LEO: close it for debugging
    // tft_area_associate(tmp);

    return tmp;
}


void tft_area_delete(tft_area_t *area)
{
    if (area == NULL)
        return;
    tft_batch_t *batch = area->batch; 

    if(batch == NULL || batch->areaHash == NULL || area == NULL) {
        LOGE("ERROR: call tft_area_delete with null param. \
              batch=%p, areaHash=%p, area=%p\n",\
              batch, batch->areaHash, area);
        return;
    }

    LOGD("tft_area_delete in batch ref=%ld with name=%s, type=%d\n",
          batch->ref, area->name, area->atype);

    if(area->atype == TFT_AREA_TYPE_APP) {
        LOGD("tft_area_delete delete app area: %s\n", area->name);
        /* only remove app area and scence area counter */
        hashDelK(batch->areaHash, area->name);
        batch->appArea = NULL;
    }

    if(area->atype == TFT_AREA_TYPE_SCENCE) {
        LOGD("tft_area_delete delete scence area: %s\n", area->name);
        /* only remove app area and scence area counter */
        hashDelK(batch->areaHash, area->name);
        batch->scenceArea = NULL;
    }

    /* do NOT remove subarea counter to avoid conflict */
    // hashDelK(batch->areaHash, area->name);

    /* remove form batch->areas linker and free */
    tft_area_free(area);
}


/* create new subarea in exist tft batch */
tft_area_t * tft_area_new(tft_batch_t *batch, enum tft_area_enum type, char *name, int x1, int y1, int x2, int y2)
{
    tft_area_t *area;
    if(batch == NULL || batch->areaHash == NULL ) {
        LOGE("ERROR: call tft_area_new with null batch. \
              batch=%p, areaHash=%p\n",\
              batch, batch->areaHash);
        return NULL;
    }

    LOGD("tft_area_new in batch ref=%ld with name=%s, \
          batch.app=%p, batch.scence=%p, areaHash=%p\n",\
          batch->ref, name, batch->appArea, batch->scenceArea, batch->areaHash);

    area = tft_area_alloc(batch, name, type, x1,y1,x2,y2);
    /* update batch app or scence area pointer */
    if (area != NULL) {
        if (type == TFT_AREA_TYPE_APP)
            batch->appArea = area;
        else if (type == TFT_AREA_TYPE_SCENCE)
            batch->scenceArea = area;
    }
    return area;
}

/* update exist tft batch */
void tft_area_update(tft_area_t *area, int x1, int y1, int x2, int y2)
{
    if(area == NULL) {
        LOGE("tft_area_update called with NULL area param\n");
        return;
    }
    area->xmin = x1;
    area->ymin = y1;
    area->xmax = x2;
    area->ymax = y2;
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

tft_batch_t * tft_batch_alloc(tft_buffer_t *buf, long ref)
{
    tft_batch_t *tmp;

    if (buf == NULL) {
        LOGE("ERROR: call tft_batch_alloc with null buf.\n");
        return NULL;
    }

    tmp = (tft_batch_t *)malloc(sizeof(tft_batch_t));
    if (tmp != NULL) {
        tmp->prev = tmp;
        tmp->next = tmp;
        tmp->areas = NULL;
        tmp->buffer = buf;
        tmp->ref = ref;
        tmp->appArea = NULL;
        tmp->scenceArea = NULL;
        /* create area hashtab inside batch */
        tmp->areaHash = hashInit(TFT_AREA_NUM_MAX);
        assert(tmp->areaHash != NULL);

        /* insert into linker and hashtab */
        if (buf->batchs == NULL) buf->batchs = tmp;
        else tft_batch_insert(buf->batchs, tmp);
        hashLongSetP(buf->batchHash, ref, (void*)tmp);
    }
    return tmp;
}

tft_batch_t * tft_batch_load(json_t *unit, tft_buffer_t *buffer)
{
    unsigned int i;
    long ref = 1;
    tft_batch_t *tmp;
    json_t *subunit, *aunit, *key;
    char *name;
    int x1,y1,x2,y2;


    assert(json_typeof(unit) == JSON_OBJECT);
    subunit = json_object_get(unit, "ref");
    assert(json_typeof(subunit) == JSON_INTEGER);
    ref = json_integer_value(subunit);

    tmp = tft_batch_alloc(buffer, ref);
    if (tmp == NULL) {
        LOGE("tft_batch_load: out of memory.\n");
        return NULL;
    }

    subunit = json_object_get(unit, "app");
    assert(json_typeof(subunit) == JSON_STRING);
    name = (char *)json_string_value(subunit);
    tft_area_new(tmp, TFT_AREA_TYPE_APP, name, 0,0,0,0);

    subunit = json_object_get(unit, "scence");
    assert(json_typeof(subunit) == JSON_STRING);
    name = (char *)json_string_value(subunit);
    tft_area_new(tmp, TFT_AREA_TYPE_SCENCE, name, 0,0,0,0);

    subunit = json_object_get(unit, "subareas");
    assert(json_typeof(subunit) == JSON_ARRAY);
    for(i=0; i<json_array_size(subunit); i++)
    {
        aunit = json_array_get(subunit, i);
        assert(json_typeof(aunit) == JSON_OBJECT);
        key = json_object_get(aunit, "type");
        assert(json_typeof(key) == JSON_STRING);
        name = (char *)json_string_value(key);

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
        
        tft_area_new(tmp, TFT_AREA_TYPE_SUBAREA, name, x1, y1, x2, y2);
    }
    return tmp;
}


/* update appArea or scenceArea */
tft_batch_t * tft_batch_update(tft_buffer_t *buf, long ref, char *appName, char *scenceName)
{
    tft_batch_t *batch;
    hashLongItem *item;
    if (buf == NULL || buf->batchHash == NULL) {
        LOGE("ERROR: call tft_batch_new with null buf. \
              buf=%p, batchHash=%p \n",\
              buf, buf->batchHash);
        return NULL;
    }

    item = hashLongGetP(buf->batchHash, ref);
    /* create new batch */
    if (item == NULL) {
        LOGD("tft_batch_update: new batch with ref:%ld with app: %s\n", ref, appName);
        batch = tft_batch_alloc(buf, ref);
        if (batch == NULL) {
            LOGE("tft_batch_update: out of memory.\n");
            return NULL;
        }
        if (appName != NULL) 
            tft_area_new(batch, TFT_AREA_TYPE_APP, appName, 0,0,0,0);
        if (scenceName != NULL)
            tft_area_new(batch, TFT_AREA_TYPE_SCENCE, scenceName, 0,0,0,0);
    }
    /* update exist batch */
    else {
        batch = item->pval;
        assert(batch != NULL);
        /* update app area */
        if (appName != NULL) {
            if (batch->appArea == NULL) {
                LOGD("tft_batch_update: update %ld with new \
                    app: %s from old NULL app\n", ref, appName);
                tft_area_new(batch, TFT_AREA_TYPE_APP, appName, 0,0,0,0);
            }
            else if (strcmp(appName, batch->appArea->name) != 0) {
                LOGD("tft_batch_update: update %ld with new \
                    app: %s from old app: %s\n", ref, appName, batch->appArea->name);
                tft_area_delete(batch->appArea);
                tft_area_new(batch, TFT_AREA_TYPE_APP, appName, 0,0,0,0);
            }
        }
        /* update scence area */
        else if (scenceName != NULL) {
            if (batch->scenceArea == NULL) {
                LOGD("tft_batch_update: update %ld with new \
                    scence: %s from old NULL scence\n", ref, scenceName);
                tft_area_new(batch, TFT_AREA_TYPE_SCENCE, scenceName, 0,0,0,0);
            }
            else if (strcmp(scenceName, batch->scenceArea->name) != 0) {
                LOGD("tft_batch_update: update %ld with new \
                    scence: %s from old scence: %s\n", ref, scenceName, batch->scenceArea->name);
                tft_area_delete(batch->scenceArea);
                tft_area_new(batch, TFT_AREA_TYPE_SCENCE, scenceName, 0,0,0,0);
            }
        }
    }
    
    return batch;
}

void tft_box_active(tft_buffer_t *tftBuffer, long ref)
{
    tft_area_t *area, *area0;
    tft_box_t *box;
    hashLongItem *item;
    tft_batch_t *tftBatch;
    hashLongTab *tftBatchHash;

    if(tftBuffer == NULL) {
        LOGE("TFT BUFFER is empty.\n");
        return;
    }

    tft_box_reset(tftBuffer);
    
    LOGI("------TFT Batch ref: %ld  \n", ref);
    tftBatchHash = tftBuffer->batchHash;
    item = hashLongGetP(tftBatchHash, ref);
    if(item == NULL) {
        LOGI("----NO MATCH TFT\n");
        tft_batch_update(tftBuffer, ref, NULL, NULL);
        return;
    }
    tftBatch = (tft_batch_t *)item->pval;


    area0 = tftBatch->areas;
    area = area0;
    if(area == NULL) {
        LOGD("tft_box_active called with NULL area\n");
        return;
    }
    do {
        /* try associate dynamic */
        if (area->box == NULL)
            tft_area_associate(area);
        if (area->box == NULL) {
            LOGD("tft_box_active failed for area: %s, seq: %d \n",
                area->name, area->seq);
            continue;
        }
        box = area->box;
        box->status = 1;
        if (box->sceneitem != NULL)
            obs_sceneitem_set_visible(box->sceneitem, true);
        box->area = area;

        area = area->next;
    }while(area != area0);
}

tft_buffer_t * tft_buffer_load(obs_scene_t *scene, const char *jsonfile)
{
    unsigned int i;
    struct tft_buffer *buffer;
    tft_batch_t *new;
    json_t *unit;
    json_t *root;
    json_error_t error;

    /* load ai result from json file */
    root = json_load_file(jsonfile, 0, &error);
    if(root == NULL) {
        fprintf(stderr, "Error MSG: %s\n", error.text);
        return NULL;
    }

    assert(json_typeof(root) == JSON_OBJECT);
    buffer = (tft_buffer_t*)malloc(sizeof(tft_buffer_t));
    assert(buffer != NULL);
    
    buffer->scene = scene;

    /* init batchHash and batchLinker */
    buffer->batchHash = hashLongInit(TFT_BATCH_NUM_MAX);
    buffer->batchs = NULL;
    
    unit = json_object_get(root, "name");
    assert(json_typeof(unit) == JSON_STRING);
    buffer->name = strdup(json_string_value(unit));

    unit = json_object_get(root, "batchs");
    assert(json_typeof(unit) == JSON_ARRAY);

    /* init code boxHash */
    buffer->boxes = NULL;
    buffer->boxHash = hashInit(TFT_BOX_NUM_MAX);

    for(i=0; i<json_array_size(unit); i++)
    {
        new = tft_batch_load(json_array_get(unit, i), buffer);
        if (new == NULL) {
            LOGE("tft_buffer_load: out of memory.\n");
            return NULL;
        }
    }
    json_decref(root);
    return buffer;
}

void tft_buffer_free(tft_buffer_t *buffer)
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

#if 0

void obs_buffer_print(obs_buffer_t *buf)
{
    char * name;
    obs_batch_t *batch, *batch0;
    tft_box_t *box, *box0;
    if(buf == NULL) {
        LOGE("OBS BUFFER is empty.\n");
        return;
    }

    name = buf->name;
    LOGI("*************OBS Buffer: %s ***************\n", name);
    batch0 = buf->batchs;
    batch = batch0;
    do {
        assert(batch != NULL);
        LOGI("------Batch ref: %ld\n", batch->ref);
        LOGI("xlen: %4d, ylen: %4d\n", batch->xlen, batch->ylen);
        batch = batch->next;
    }while(batch != batch0);

    box0 = buf->boxes;
    box = box0;
    do {
        if (box == NULL) continue;
        LOGI("------Box name: %s\n", box->name);
        box = box->next;
    }while(box != box0);
}

#endif

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
    LOGI("*************TFT Buffer: %s ***************\n", name);
    batch0 = buf->batchs;
    batch = batch0;
    do {
        assert(batch != NULL);
        LOGI("------Batch ref: %ld\n", batch->ref);
        LOGI("------app.scence: %s.%s\n", batch->appArea->name, batch->scenceArea->name);

        area0 = batch->areas;
        area = area0;
        do {
            assert(area != NULL);
            LOGI("----area name: type[%d]-%s-seq[%d]\n", area->atype, area->name, area->seq);
            LOGI("(xmin,ymin,xmax,ymax): (%d,%d,%d,%d)\n",area->xmin,area->ymin,area->xmax,area->ymax);
            area = area->next;
        }while(area != area0);

        batch = batch->next;
    }while(batch != batch0);
}


void test_run(tft_buffer_t *tftBuffer)
{
    tft_batch_t *tftBatch;

    for (int i=0; i<3; i++) {
        LOGI("*************Run obs Buffer***************\n");
        for (long ref=2018030602102; ref<2018030602199; ref+=10) {
            tft_box_active(tftBuffer, ref);
            tft_box_show(tftBuffer);
            sleep(1);
        }
        /* test tft update */
        tftBatch = tft_batch_update(tftBuffer, 2018030602102, "newgggcode", NULL);
        tft_batch_update(tftBuffer, 20190302153002, "pigie", NULL);
        tftBatch = tft_batch_update(tftBuffer, 20190302153003, NULL, "red");
        if(tftBatch != NULL)
            tft_area_new(tftBatch, TFT_AREA_TYPE_SUBAREA, "i like coding", 99,99,999,999);
        tftBatch = tft_batch_update(tftBuffer, 20190302153004, NULL, NULL);
        if(tftBatch != NULL)
            tft_area_delete(tftBatch->appArea);
        if(tftBatch != NULL)
            tft_area_new(tftBatch, TFT_AREA_TYPE_SUBAREA, "what is this code", 99,99,999,999);
    }
}


#if 0

int main(int argc, char *argv[]) {
    tft_buffer_t *tftBuffer;

    if (argc != 1) {
        fprintf(stderr, "Usage: %s\n", argv[0]);
        exit(-1);
    }

    /* init a demo buffer */
//    obsBuffer = obs_buffer_load();


    tftBuffer = tft_buffer_load("./main.json");
    
//    obs_buffer_print(obsBuffer);
    tft_buffer_print(tftBuffer);

    test_run(tftBuffer);
    

    return 0;
}

#endif
