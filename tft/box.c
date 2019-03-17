/******************************************************************************
Utility of tag data process for tfrecord generation.
leo, lili
******************************************************************************/


#include <jansson.h>
#include "tft.h"


#define LOGD(FORMAT, args...)   printf(FORMAT, ##args)
#define LOGI(FORMAT, args...)   printf(FORMAT, ##args)
#define LOGE(FORMAT, args...)   printf(FORMAT, ##args)

/* insert new into a bi-direction linker */
void obs_box_insert(obs_box_t *old, obs_box_t *new)
{
    obs_box_t *tmpnext;
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

obs_box_t * obs_box_alloc(char *name)
{
    obs_box_t *box = (obs_box_t *)malloc(sizeof(obs_box_t));
    box->name = strdup(name);
    box->status = 0;
    box->prev = box;
    box->next = box;
    box->area = NULL;
    box->handle = NULL;
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
    tft_area_t *area;

    if(buf == NULL) return;

    box0 = buf->boxes;
    box = box0;
    do {
        if(box == NULL) {
            LOGD("obs_boxes_show: NO box now.\n");
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


void obs_batch_active(tft_buffer_t *tftBuffer, long ref)
{
    tft_area_t *area, *area0;
    obs_box_t *box;
    hashLongItem *item;
    tft_batch_t *tftBatch;
    hashLongTab *tftBatchHash;
    obs_buffer_t *obsBuffer;

    assert(tftBuffer != NULL);
    obsBuffer = tftBuffer->obsBuffer;
    if(obsBuffer == NULL) {
        LOGE("OBS BUFFER is empty.\n");
        return;
    }

    obs_buffer_reset(obsBuffer);
    LOGI("------OBS Batch ref: %ld  ", ref);
    tftBatchHash = tftBuffer->batchHash;
    item = hashLongGetP(tftBatchHash, ref);
    if(item == NULL) {
        LOGI("----NO MATCH TFT\n");
        return;
    }
    tftBatch = (tft_batch_t *)item->pval;

    area0 = tftBatch->areas;
    area = area0;
    do {
        assert(area != NULL);
        /* try associate dynamic */
        if (area->box == NULL)
            tft_area_associate(area);
        if (area->box == NULL) {
            LOGD("obs_batch_active failed for area: %s, seq: %d \n",
                area->name, area->seq);
            continue;
        }
        box = area->box;
        box->status = 1;
        box->area = area;

        area = area->next;
    }while(area != area0);
}

