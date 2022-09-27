#pragma once

#include "../../include/libCacheSim/evictionAlgo/GLCache.h"
#include "bucket.h"
#include "const.h"
#include "obj.h"
#include "segment.h"

/************* feature *****************/
void seg_feature_shift(GLCache_params_t *params, segment_t *seg);

void seg_hit_update(GLCache_params_t *params, cache_obj_t *cache_obj);

/************* training *****************/


/************* inference *****************/

void train(cache_t *cache);

void inference(cache_t *cache);

/************* data preparation *****************/
void dump_training_data(cache_t *cache); 

void snapshot_segs_to_training_data(cache_t *cache); 

void update_train_y(GLCache_params_t *params, cache_obj_t *cache_obj); 

void prepare_training_data(cache_t *cache); 

