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

typedef struct vec2 vec2_t;

static obs_sceneitem_t *null_obs_scene_find_source(obs_scene_t *scene, const char *name)
{
    return NULL;
}

static bool _sceneitem_set_invisible(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
    const char *id;
    obs_source_t *source = obs_sceneitem_get_source(item);
    id = obs_source_get_id(source); 
    if (id != NULL && strcmp("obs_box", id) == 0)
        obs_sceneitem_set_visible(item, false);

    UNUSED_PARAMETER(scene);
    UNUSED_PARAMETER(param);
    return true;
}

static bool _sceneitem_remove_all_box(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
    const char *id;
    obs_source_t *source = obs_sceneitem_get_source(item);
    id = obs_source_get_id(source); 
    if (id != NULL && (strcmp("obs_box", id)==0 || 
                       strcmp("text_ft2_source",id)==0)) {
     //   obs_sceneitem_remove(item);
        /* need deselect before delete */
        //obs_sceneitem_select(item, false);
        obs_source_remove(source);
    }

    UNUSED_PARAMETER(scene);
    UNUSED_PARAMETER(param);
    return true;
}

static bool _sceneitem_remove_all_group(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
    const char *id;
    obs_source_t *source = obs_sceneitem_get_source(item);
    id = obs_source_get_id(source); 
    if (id != NULL && strcmp("group",id)==0 ) {
        /* need deselect before delete */
        //obs_sceneitem_select(item, false);
        obs_sceneitem_group_ungroup(item);
    }

    UNUSED_PARAMETER(scene);
    UNUSED_PARAMETER(param);
    return true;
}

static char * tft_new_name(const char *name, int seq)
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

static tft_area_t * tft_area_alloc(tft_batch_t *batch, const char *name, 
        enum tft_area_enum type, struct vec2 pos, uint32_t width, uint32_t height)
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
    tmp->pos = pos;
    tmp->width = width;
    tmp->height = height;

    /* add counter hash for same name */
    item = hashGetI(batch->areaSeqHash, name);
    if (item != NULL) tmp->seq = item->ival + 1;
    else tmp->seq = 1;
    LOGD("hashGetI(%s)=%d\n",name,tmp->seq);
    assert(hashSetI(batch->areaSeqHash, name, tmp->seq)>=0);

    tmp->fullname = tft_new_name(name, tmp->seq);
    assert(hashSetP(batch->areaPtrHash, tmp->fullname, tmp)>=0);

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

static tft_area_t * tft_area_get_from_name(tft_batch_t *batch, const char *name)
{
    tft_area_t *tmp = NULL;
    hashItem *item;

    item = hashGetP(batch->areaPtrHash, name);
    if (item != NULL)
        tmp = item->pval;
    return tmp;
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

static tft_batch_t * tft_batch_get_from_ref(tft_buffer_t *tftBuffer, long ref)
{
    tft_batch_t *tmp = NULL;
    hashLongItem *item;

    item = hashLongGetP(tftBuffer->batchHash, ref);
    if (item != NULL)
        tmp = item->pval;
    return tmp;
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
        tmp->width = 0;
        tmp->height = 0;
        tmp->appArea = NULL;
        tmp->sceneArea = NULL;
        /* create area hashtab for seq inside batch */
        tmp->areaSeqHash = hashInit(TFT_AREA_NUM_MAX);
        assert(tmp->areaSeqHash != NULL);
        /* create area hashtab for area pointer inside batch */
        tmp->areaPtrHash = hashInit(TFT_AREA_NUM_MAX);
        assert(tmp->areaPtrHash != NULL);

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

void tft_area_delete(tft_area_t *area)
{
    if (area == NULL)
        return;
    tft_batch_t *batch = area->batch; 

    if(batch == NULL || batch->areaPtrHash == NULL || area == NULL) {
        LOGE("ERROR: call tft_area_delete with null param. \
              batch=%p, areaPtrHash=%p, area=%p\n",\
              batch, batch->areaPtrHash, area);
        return;
    }

    LOGD("tft_area_delete in batch ref=%ld with name=%s, type=%d\n",
          batch->ref, area->name, area->atype);

    /* Lock and process this batch */
	pthread_mutex_lock(&batch->mutex);


    if(area->atype == TFT_AREA_TYPE_APP) {
        LOGD("tft_area_delete delete app area: %s\n", area->name);
        /* only remove app area and scene area counter */
        hashDelK(batch->areaSeqHash, area->name);
        hashDelK(batch->areaPtrHash, area->fullname);
        batch->appArea = NULL;
    }

    if(area->atype == TFT_AREA_TYPE_SCENE) {
        LOGD("tft_area_delete delete scene area: %s\n", area->name);
        /* only remove app area and scene area counter */
        hashDelK(batch->areaSeqHash, area->name);
        hashDelK(batch->areaPtrHash, area->fullname);
        batch->sceneArea = NULL;
    }

    /* do NOT remove subarea counter to avoid conflict */
    // hashDelK(batch->areaSeqHash, area->name);

    /* remove form batch->areas linker and free */
    tft_area_free(area);

	pthread_mutex_unlock(&batch->mutex);
}


/* create new subarea in exist tft batch */
tft_area_t * tft_area_new(tft_batch_t *batch, enum tft_area_enum type, 
       const char *name, struct vec2 pos, uint32_t width, uint32_t height)
{
    tft_area_t *area;

    if(batch == NULL || batch->areaSeqHash == NULL || batch->areaPtrHash == NULL) {
        LOGE("ERROR: call tft_area_new with null batch. \
              batch=%p, areaSeqHash=%p, areaPtrHash=%p\n",\
              batch, batch->areaSeqHash, batch->areaPtrHash);
        return NULL;
    }

    LOGD("tft_area_new in batch ref=%ld with name=%s, \
          batch.app=%p, batch.scene=%p, areaSeqHash=%p, areaPtrHash=%p\n",\
          batch->ref, name, batch->appArea, batch->sceneArea, \
          batch->areaSeqHash, batch->areaSeqHash);

    /* Lock and process this batch */
	pthread_mutex_lock(&batch->mutex);

    area = tft_area_alloc(batch, name, type, pos, width, height);
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
        else if (type == TFT_AREA_TYPE_SCENE) {
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

/* update exist tft area from source */
void tft_area_update_from_source(tft_buffer_t *tftBuffer, obs_source_t *source)
{
    const char * fullname = obs_source_get_name(source);
    tft_batch_t * batch = tft_batch_get_from_ref(tftBuffer, tftBuffer->curRef);
    tft_area_t * area = tft_area_get_from_name(batch, fullname);
    obs_sceneitem_t * sceneitem = null_obs_scene_find_source(tftBuffer->scene, fullname);

    if (sceneitem == NULL || area == NULL) {
        LOGE("tft_area_update_from_source called with NULL sceneitem or area.\n");
        return;
    }
    /* update area position from sceneitem */
	pthread_mutex_lock(&batch->mutex);
    obs_sceneitem_get_pos(sceneitem, &area->pos);
    area->width = obs_source_get_width(source);
    area->height = obs_source_get_height(source);
	pthread_mutex_unlock(&batch->mutex);

    LOGD("tft_area_update_from_source: x=%f, y=%f\n, w=%d, h=%d", area->pos.x,
        area->pos.y, area->width, area->height);
}

/* update exist tft area */
void tft_area_update(tft_area_t *area, struct vec2 pos, uint32_t width, uint32_t height)
{
    if(area == NULL) {
        LOGE("tft_area_update called with NULL area param\n");
        return;
    }
    /* Lock and process this batch */
	pthread_mutex_lock(&area->batch->mutex);
    area->pos = pos;
    area->width = width;
    area->height = height;
	pthread_mutex_unlock(&area->batch->mutex);
}

tft_batch_t * tft_batch_load(json_t *unit, tft_buffer_t *buffer)
{
    unsigned int i;
    long ref = 1;
    tft_batch_t *tmp;
    json_t *subunit, *aunit, *key;
    char *name;
    struct vec2 pos = {0,0};
    uint32_t width;
    uint32_t height;


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
    tft_area_new(tmp, TFT_AREA_TYPE_APP, name, pos,0,0);

    subunit = json_object_get(unit, "scene");
    assert(json_typeof(subunit) == JSON_STRING);
    name = (char *)json_string_value(subunit);
    tft_area_new(tmp, TFT_AREA_TYPE_SCENE, name, pos,0,0);

    subunit = json_object_get(unit, "subareas");
    assert(json_typeof(subunit) == JSON_ARRAY);
    for(i=0; i<json_array_size(subunit); i++)
    {
        aunit = json_array_get(subunit, i);
        assert(json_typeof(aunit) == JSON_OBJECT);
        key = json_object_get(aunit, "type");
        assert(json_typeof(key) == JSON_STRING);
        name = (char *)json_string_value(key);

        key = json_object_get(aunit, "x");
        assert(json_typeof(key) == JSON_INTEGER);
        pos.x = (float)json_integer_value(key);
        key = json_object_get(aunit, "y");
        assert(json_typeof(key) == JSON_INTEGER);
        pos.y = (float)json_integer_value(key);
        key = json_object_get(aunit, "width");
        assert(json_typeof(key) == JSON_INTEGER);
        width = (unsigned)json_integer_value(key);
        key = json_object_get(aunit, "height");
        assert(json_typeof(key) == JSON_INTEGER);
        height = (unsigned)json_integer_value(key);
        
        tft_area_new(tmp, TFT_AREA_TYPE_SUBAREA, name, pos, width, height);
    }
    return tmp;
}

static bool __area_update_from_sceneitem(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
    const char *id;
    const char *fullname;
    obs_source_t *source;
    struct vec2 pos;
    tft_buffer_t *tftBuffer = param;
    tft_batch_t  *tftBatch;
    source = obs_sceneitem_get_source(item); 
    id = obs_source_get_id(source);
    /* LEO: bug, should not use fullname */
    fullname = obs_source_get_name(source);
    if (id == NULL || strcmp("obs_box", id)!=0 || !obs_sceneitem_visible(item)) 
        return true;

    obs_sceneitem_get_pos(item, &pos);
    uint32_t height = obs_source_get_height(source);
    uint32_t width = obs_source_get_width(source);

    tftBatch = tft_batch_update(tftBuffer, tftBuffer->curRef, NULL, NULL);
    tft_area_t * area = tft_area_get_from_name(tftBatch, fullname);
    if (area == NULL) {
        if(tftBatch != NULL) {
            tft_area_new(tftBatch, TFT_AREA_TYPE_SUBAREA, fullname, pos,width,height);
        }
    }
    else {
        /* update area position from sceneitem */
        pthread_mutex_lock(&tftBatch->mutex);
        area->pos = pos;
        area->width = width;
        area->height = height;
        pthread_mutex_unlock(&tftBatch->mutex);

        LOGD("tft_area: %s, update_from_source: x=%f, y=%f, w=%d, h=%d\n", 
              fullname, pos.x, pos.y, width, height);
    }

    UNUSED_PARAMETER(scene);
    return true;
}

/* update current batch's areas from obs scene */
void tft_batch_update_from_obs(tft_buffer_t *tftBuffer)
{
//    struct vec2 pos;
//    tft_batch_t * batch = tft_batch_get_from_ref(tftBuffer, tftBuffer->curRef);

    /* reset all box invisible in current scene */
	obs_scene_enum_items(tftBuffer->scene, __area_update_from_sceneitem, tftBuffer);

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
    struct vec2 pos = {0,0};
    /* update app area */
    if (appName != NULL) 
        tft_area_new(batch, TFT_AREA_TYPE_APP, appName, pos,0,0);
    /* update scene area */
    if (sceneName != NULL)
        tft_area_new(batch, TFT_AREA_TYPE_SCENE, sceneName, pos,0,0);
    
    return batch;
}

static obs_sceneitem_t * group_update_or_create(obs_scene_t *scene, const char *name, 
                         struct vec2 pos, uint32_t width, uint32_t height)
{
    obs_source_t *source = NULL;
    obs_sceneitem_t *boxitem = NULL;
    obs_sceneitem_t *txtitem = NULL;
    obs_sceneitem_t *group = NULL;

    static char tmpName[200];
    snprintf(tmpName, 199, "tit:%s", name);
    /* try find exist box title */
    txtitem = null_obs_scene_find_source(scene, tmpName);
    if (txtitem == NULL) {
        /* create obs_box */
        obs_data_t *txtcfg = obs_data_create();
        obs_data_set_string(txtcfg, "text", name);
        obs_data_t *font_obj = obs_data_create();
        obs_data_set_int(font_obj, "size", 12);
        obs_data_set_obj(txtcfg, "font", font_obj);
        obs_data_set_int(txtcfg, "color1", 0xFF000000);
        obs_data_set_int(txtcfg, "color2", 0xFF000000);
        source = obs_source_create("text_ft2_source", tmpName, txtcfg, NULL);
        obs_data_release(txtcfg);
        txtitem = obs_scene_add(scene, source);
        //obs_sceneitem_set_pos(txtitem, &pos);
        obs_source_release(source);
    }
    obs_sceneitem_set_visible(txtitem, true);


    /* create settings for box create and update */
    obs_data_t *settings = obs_data_create();
    obs_data_set_int(settings, "color", 0xff808080);
    obs_data_set_int(settings, "width", width);
    obs_data_set_int(settings, "height", height);

    /* try find exist box */
    boxitem = null_obs_scene_find_source(scene, name);
    if (boxitem == NULL) {
        /* create obs_box */
        source = obs_source_create("obs_box", name, settings, NULL);
        if (source == NULL) {
            LOGE("create obs_box:%s failed.\n", name);
            return NULL;
        }
        /* create opcity filter */
        obs_data_t *fset = obs_data_create();
        obs_data_set_int(fset, "opacity", 20);
        obs_source_t *filter = obs_source_create("chroma_key_filter",
            "opcity filter", fset, NULL);
        obs_data_release(fset);
        if (filter == NULL)
            LOGE("create opcity filter failed.\n");
        else {
            obs_source_filter_add(source, filter);
            obs_source_release(filter);
        }

        /* add box into scene */
        boxitem = obs_scene_add(scene, source);
        obs_source_release(source);
    }
    else {
        /* update obs_obx settings */
        LOGD("obs_box reused: %s\n", name);
        source = obs_sceneitem_get_source(boxitem);
        obs_source_update(source, settings);
        //SHOULD NOT call obs_source_release after obs_sceneitem_get_source
        obs_sceneitem_set_visible(boxitem, true);
    }
    /* release settings data memory */
    obs_data_release(settings);
    /* update box pos */
    obs_sceneitem_set_pos(boxitem, &pos);


    /* try find exist group */
    /* bugfix: using obs_sceneitem_get_group will cause dead lock */
    // group = obs_sceneitem_get_group(scene, boxitem);
    snprintf(tmpName, 199, "group:%s", name);
    group = obs_scene_get_group(scene, tmpName);
    if (group == NULL) {
        /* create new group */
        group = obs_scene_add_group(scene, tmpName);
        obs_sceneitem_set_pos(group, &pos);
//        group = obs_scene_insert_group(scene, tmpName, &boxitem, 1);
    }
/*
    obs_sceneitem_group_add_item(group, boxitem);
    obs_sceneitem_group_remove_item(group, boxitem);
    obs_sceneitem_set_visible(group, true);
    obs_sceneitem_group_add_item(group, boxitem);
    obs_sceneitem_group_remove_item(group, boxitem);
    obs_sceneitem_group_add_item(group, boxitem);
    obs_sceneitem_set_visible(group, true);
*/
    obs_sceneitem_group_add_item(group, boxitem);
    obs_sceneitem_group_add_item(group, txtitem);
    obs_sceneitem_set_visible(group, true);

    return group;
}

/* release buffer and exit */
void tft_exit(tft_buffer_t *tftBuffer)
{
    /* BUG: force to remove all box and text in current scene */
	obs_scene_enum_items(tftBuffer->scene, _sceneitem_remove_all_group, NULL);
	obs_scene_enum_items(tftBuffer->scene, _sceneitem_remove_all_box, NULL);
}

/* new active which will recreate everything without using cache */
void tft_batch_set(tft_buffer_t *tftBuffer, long ref, uint32_t batchW, uint32_t batchH)
{
    hashLongTab *tftBatchHash;
    hashLongItem *item;
    tft_batch_t *tftBatch;

    if(tftBuffer == NULL || tftBuffer->scene == NULL) {
        LOGE("tft_batch_set: TFT BUFFER or SCENE is empty.\n");
        return;
    }

    tftBatchHash = tftBuffer->batchHash;
    item = hashLongGetP(tftBatchHash, ref);
    if(item == NULL) {
        LOGI("tft_batch_set:----NO MATCH TFT\n");
        tftBatch = tft_batch_update(tftBuffer, ref, NULL, NULL);
    }
    else
        tftBatch = (tft_batch_t *)item->pval;

    /* Lock and process this batch */
	pthread_mutex_lock(&tftBatch->mutex);
    tftBatch->width = batchW;
    tftBatch->height = batchH;
	pthread_mutex_unlock(&tftBatch->mutex);

    tftBuffer->setRef = ref;
}

/* new active which will recreate everything without using cache */
void tft_batch_active(tft_buffer_t *tftBuffer)
{
    obs_sceneitem_t *sceneitem;
    obs_source_t *source;
    hashLongItem *item;
    tft_batch_t *tftBatch;
    hashLongTab *tftBatchHash;
    uint32_t width, height;
    long ref;

    if(tftBuffer == NULL || tftBuffer->scene == NULL) {
        LOGE("TFT BUFFER or SCENE is empty.\n");
        return;
    }

    //LOGI("------tft_batch_active: curRef=%ld, setRef=%ld\n", tftBuffer->curRef, tftBuffer->setRef);
    /* does it need update? */
    if (tftBuffer->curRef == tftBuffer->setRef)
        return;
    tftBuffer->curRef = tftBuffer->setRef;
    ref = tftBuffer->curRef;

    /* BUG: force to remove all box and text in current scene */
    /* SHOULD remove items before remove group */
	obs_scene_enum_items(tftBuffer->scene, _sceneitem_remove_all_group, NULL);
	obs_scene_enum_items(tftBuffer->scene, _sceneitem_remove_all_box, NULL);
    
    LOGI("------TFT Batch ref: %ld  \n", ref);
    tftBatchHash = tftBuffer->batchHash;
    item = hashLongGetP(tftBatchHash, ref);
    if(item == NULL) {
        LOGI("----NO MATCH TFT\n");
        tftBatch = tft_batch_update(tftBuffer, ref, NULL, NULL);
    }
    else
        tftBatch = (tft_batch_t *)item->pval;

    /* Lock and process this batch */
	pthread_mutex_lock(&tftBatch->mutex);

    char *appName = tftBatch->appArea==NULL?NULL:tftBatch->appArea->name;
    char *sceneName = tftBatch->sceneArea==NULL?NULL:tftBatch->sceneArea->name;
    char batchName[200];
    snprintf(batchName, 199, "ref:%ld app:%s scene:%s", tftBatch->ref, appName, sceneName);
//    uint32_t nameLen = strlen(batchName);
//    float xpos = batchW<nameLen?0:(batchW-nameLen)/2;
    struct vec2 pos = {100, 0};

    obs_data_t *settings = obs_data_create();
    obs_data_set_string(settings, "text", batchName);
	obs_data_set_int(settings, "color1", 0xFF000000);
	obs_data_set_int(settings, "color2", 0xFF000000);
    /* use defalt size = 32 */
    source = obs_source_create("text_ft2_source", "batch", settings, NULL);
    obs_data_release(settings);
    sceneitem = obs_scene_add(tftBuffer->scene, source);
    obs_sceneitem_set_pos(sceneitem, &pos);
    obs_source_release(source);
    //obs_sceneitem_select(sceneitem, false);

    tft_area_t *area, *area0;
    area0 = tftBatch->areas;
    area = area0;
    if(area == NULL) {
        LOGD("tft_batch_active called with NULL area\n");
        pthread_mutex_unlock(&tftBatch->mutex);
        return;
    }
    
    do {
        /* do not need show app and scene */
        if (area->atype == TFT_AREA_TYPE_SUBAREA) {
            width = (area->width!=0)?area->width:tftBatch->width;
            height = (area->height!=0)?area->height:tftBatch->height;
            sceneitem = group_update_or_create(tftBuffer->scene, area->fullname, area->pos,
                                    width, height);
            LOGD("tft_batch:%ld actived sceneitem:%s,pos=(%f,%f),w-h=(%d,%d)\n",\
                    ref, area->fullname, area->pos.x, area->pos.y, width, height); 
            area->sceneitem = sceneitem;
            
            //obs_sceneitem_select(sceneitem, false);
        }
        area = area->next;
    }while(area != area0);

	pthread_mutex_unlock(&tftBatch->mutex);

    obs_scene_sourcetree_reset(tftBuffer->scene);
/*
    obs_sceneitem_set_order(sceneitem, OBS_ORDER_MOVE_TOP);
	obs_scene_t *scene = obs_sceneitem_get_scene(sceneitem);
	obs_source_t *sceneSource = obs_scene_get_source(scene);
*/
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
    buffer->curRef = 0;
    buffer->setRef = 0;

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
            LOGI("(pos.x,pos.y,width,height): (%f,%f,%d,%d)\n",area->pos.x,area->pos.y,area->width,area->height);
            area = area->next;
        }while(area != area0);

        batch = batch->next;
    }while(batch != batch0);
}


void test_run(tft_buffer_t *tftBuffer)
{
    tft_batch_t *tftBatch;
    struct vec2 pos = {90,90};

    for (int i=0; i<3; i++) {
        LOGI("*************Run obs Buffer***************\n");
        for (long ref=2018030602102; ref<2018030602199; ref+=10) {
            tft_batch_set(tftBuffer, ref, 1000, 600);
            sleep(1);
        }
        /* test tft update */
        tftBatch = tft_batch_update(tftBuffer, 2018030602102, "newgggcode", NULL);
        tft_batch_update(tftBuffer, 20190302153002, "pigie", NULL);
        tftBatch = tft_batch_update(tftBuffer, 20190302153003, NULL, "red");
        if(tftBatch != NULL)
            tft_area_new(tftBatch, TFT_AREA_TYPE_SUBAREA, "i like coding", pos,100,20);
        tftBatch = tft_batch_update(tftBuffer, 20190302153004, NULL, NULL);
        if(tftBatch != NULL)
            tft_area_delete(tftBatch->appArea);
        if(tftBatch != NULL)
            tft_area_new(tftBatch, TFT_AREA_TYPE_SUBAREA, "what is this code", pos,500,500);
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
