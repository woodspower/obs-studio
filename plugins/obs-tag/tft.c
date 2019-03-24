/******************************************************************************
Utility of tag data process for tfrecord generation.
leo, lili
******************************************************************************/
#include "tft.h"


#define LOGD(FORMAT, args...)   printf(FORMAT, ##args)
#define LOGI(FORMAT, args...)   printf(FORMAT, ##args)
#define LOGE(FORMAT, args...)   printf(FORMAT, ##args)

/* TEST marcro */
#define TEST_BUFFER_BATCH_NUM 10
#define TEST_BUFFER_SOURCE_NUM 5
#define TEST_BUFFER_NAME "qj-2288-01"


static bool _sceneitem_set_invisible(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
    const char *id;
    id = obs_source_get_id(obs_sceneitem_get_source(item)); 
    if (id != NULL && strcmp("obs_box", id) == 0)
        obs_sceneitem_set_visible(item, false);

    UNUSED_PARAMETER(scene);
    UNUSED_PARAMETER(param);
    return true;
}

static char * tft_new_name(char *name, int seq)
{
    /* name and max 10 char len of seq number */
    char * new = (char *)malloc(strlen(name)+10);
    assert(name != NULL);
    assert(seq >= 0);
    sprintf(new, "%s%03d", name, seq);
    return new;
}


static void tft_area_insert(tft_area_t *old, tft_area_t *new)
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

static void tft_area_free(tft_area_t *item)
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

static tft_area_t * tft_area_alloc(tft_batch_t *batch, char *name, enum tft_area_enum type, int x1, int y1, int x2, int y2)
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
    tmp->sceneitem  = NULL;
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

    tmp->fullname = tft_new_name(name, tmp->seq);

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

    /* Lock and process this batch */
	pthread_mutex_lock(&batch->mutex);


    if(area->atype == TFT_AREA_TYPE_APP) {
        LOGD("tft_area_delete delete app area: %s\n", area->name);
        /* only remove app area and scene area counter */
        hashDelK(batch->areaHash, area->name);
        batch->appArea = NULL;
    }

    if(area->atype == TFT_AREA_TYPE_SCENCE) {
        LOGD("tft_area_delete delete scene area: %s\n", area->name);
        /* only remove app area and scene area counter */
        hashDelK(batch->areaHash, area->name);
        batch->sceneArea = NULL;
    }

    /* do NOT remove subarea counter to avoid conflict */
    // hashDelK(batch->areaHash, area->name);

    /* remove form batch->areas linker and free */
    tft_area_free(area);

	pthread_mutex_unlock(&batch->mutex);
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
          batch.app=%p, batch.scene=%p, areaHash=%p\n",\
          batch->ref, name, batch->appArea, batch->sceneArea, batch->areaHash);

    /* Lock and process this batch */
	pthread_mutex_lock(&batch->mutex);

    area = tft_area_alloc(batch, name, type, x1,y1,x2,y2);
    /* update batch app or scene area pointer */
    if (area != NULL) {
        if (type == TFT_AREA_TYPE_APP) {
            if (batch->appArea == NULL) {
                LOGD("tft_area_new: update %ld with new \
                    app: %s from old NULL app\n", batch->ref, name);
                batch->appArea = area;
            }
            else {
                LOGD("tft_area_new: update %ld with new \
                    app: %s from old app: %s\n", batch->ref, name, batch->appArea->name);
                tft_area_delete(batch->appArea);
                batch->appArea = area;
            }
        }
        else if (type == TFT_AREA_TYPE_SCENCE) {
            if (batch->sceneArea == NULL) {
                LOGD("tft_area_new: update %ld with new \
                    scene: %s from old NULL scene\n", batch->ref, name);
                batch->sceneArea = area;
            }
            else {
                LOGD("tft_area_new: update %ld with new \
                    scene: %s from old scene: %s\n", batch->ref, name, batch->sceneArea->name);
                tft_area_delete(batch->sceneArea);
                batch->sceneArea = area;
            }
        }
    }
	pthread_mutex_unlock(&batch->mutex);
    return area;
}

/* update exist tft batch */
void tft_area_update(tft_area_t *area, int x1, int y1, int x2, int y2)
{
    if(area == NULL) {
        LOGE("tft_area_update called with NULL area param\n");
        return;
    }
    /* Lock and process this batch */
	pthread_mutex_lock(&area->batch->mutex);
    area->xmin = x1;
    area->ymin = y1;
    area->xmax = x2;
    area->ymax = y2;
	pthread_mutex_unlock(&area->batch->mutex);
}

static void tft_batch_insert(tft_batch_t *old, tft_batch_t *new)
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

static tft_batch_t * tft_batch_alloc(tft_buffer_t *buf, long ref)
{
	pthread_mutexattr_t attr;
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
        tmp->sceneArea = NULL;
        /* create area hashtab inside batch */
        tmp->areaHash = hashInit(TFT_AREA_NUM_MAX);
        assert(tmp->areaHash != NULL);

        if (pthread_mutexattr_init(&attr) != 0 || 
            pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0) {
            LOGE("ERROR: create tft batch mutex attr failed.\n");
            return NULL;
        }
        if (pthread_mutex_init(&tmp->mutex, &attr) != 0) {
            LOGE("Failed to create tft batch mutex");
            free(tmp);
            return NULL;
        }
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

    subunit = json_object_get(unit, "scene");
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


/* update appArea or sceneArea */
tft_batch_t * tft_batch_update(tft_buffer_t *buf, long ref, char *appName, char *sceneName)
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
    }
    /* update exist batch */
    else {
        batch = item->pval;
        assert(batch != NULL);
    }
    /* update app area */
    if (appName != NULL) 
        tft_area_new(batch, TFT_AREA_TYPE_APP, appName, 0,0,0,0);
    /* update scene area */
    if (sceneName != NULL)
        tft_area_new(batch, TFT_AREA_TYPE_SCENCE, sceneName, 0,0,0,0);
    
    return batch;
}

void tft_batch_active(tft_buffer_t *tftBuffer, long ref)
{
    tft_area_t *area, *area0;
    obs_sceneitem_t *sceneitem;
    obs_source_t *source;
    hashLongItem *item;
    tft_batch_t *tftBatch;
    hashLongTab *tftBatchHash;
    struct vec2 pos;

    if(tftBuffer == NULL || tftBuffer->scene == NULL) {
        LOGE("TFT BUFFER or SCENE is empty.\n");
        return;
    }

    /* reset all box invisible in current scene */
	obs_scene_enum_items(tftBuffer->scene, _sceneitem_set_invisible, NULL);
    
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
        LOGD("tft_batch_active called with NULL area\n");
        return;
    }

    /* Lock and process this batch */
	pthread_mutex_lock(&tftBatch->mutex);
    
    do {
        /* try associate dynamic */
        sceneitem = obs_scene_find_source(tftBuffer->scene, area->fullname);
        if (sceneitem == NULL) {
            source = obs_source_create("obs_box", area->fullname, NULL, NULL);
            if (source == NULL) {
                LOGE("obs_source_create failed.\n");
                continue;
            }
            sceneitem = obs_scene_add(tftBuffer->scene, source);
        }
        else {
            pos.x = area->xmin;
            pos.y = area->ymin;
            obs_sceneitem_set_pos(sceneitem, &pos);
            obs_sceneitem_set_visible(sceneitem, true);
        }
        area->sceneitem = sceneitem;
        area = area->next;
    }while(area != area0);

	pthread_mutex_unlock(&tftBatch->mutex);
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
    (void)buffer;
    return;
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
    LOGI("*************TFT Buffer: %s ***************\n", name);
    batch0 = buf->batchs;
    batch = batch0;
    do {
        assert(batch != NULL);
        LOGI("------Batch ref: %ld\n", batch->ref);
        LOGI("------app.scene: %s.%s\n", batch->appArea->name, batch->sceneArea->name);

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
            tft_batch_active(tftBuffer, ref);
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
