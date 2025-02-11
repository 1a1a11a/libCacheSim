#include "ThreeLCache.h"
#include <algorithm>
#include "utils.h"
#include <chrono>

using namespace chrono;
using namespace std;
using namespace ThreeLCache;

void ThreeLCacheCache::train() {
    auto timeBegin = chrono::system_clock::now();
    if (booster) LGBM_BoosterFree(booster);
    DatasetHandle trainData;
    std::string params_str;
    for (const auto &pair : training_params) {
        params_str += pair.first + "=" + pair.second + " ";
    }
    params_str.pop_back();  // Remove trailing space

    // Convert to C-style string (const char *)
    const char *training_params_cstr = params_str.c_str();
    LGBM_DatasetCreateFromCSR(
            static_cast<void *>(training_data->indptr.data()),
            C_API_DTYPE_INT32,
            training_data->indices.data(),
            static_cast<void *>(training_data->data.data()),
            C_API_DTYPE_FLOAT64,
            training_data->indptr.size(),
            training_data->data.size(),
            n_feature, 
            training_params_cstr,
            nullptr,
            &trainData);

    LGBM_DatasetSetField(trainData,
                         "label",
                         static_cast<void *>(training_data->labels.data()),
                         training_data->labels.size(),
                         C_API_DTYPE_FLOAT32);
    LGBM_BoosterCreate(trainData, training_params_cstr, &booster);
    for (int i = 0; i < stoi(training_params["num_iterations"]); i++) {
        int isFinished;
        LGBM_BoosterUpdateOneIter(booster, &isFinished);
        if (isFinished) {
            break;
        }
    }
    LGBM_DatasetFree(trainData);

    pred_map.clear();
    pred_times.clear();
    pred_times.shrink_to_fit();

    MAX_EVICTION_BOUNDARY[0] = MAX_EVICTION_BOUNDARY[1];
    origin_current_seq = current_seq;

    if (n_req > pow(10, 6) && is_full) {
        if ((n_window_hit - n_hit) * 1.0 / (n_hit * (hsw - 1)) > 0.01) { 
            if (hsw - 1 < (n_req - n_hit) / (n_window_hit - n_hit))
                hsw += 1, is_full = 0;
            hsw = fmin(hsw, 6);
        }    
        n_hit = 0, n_window_hit = 0, n_req = 0;
    }
}

void ThreeLCacheCache::sample() {
    auto rand_idx = _distribution(_generator);
    uint32_t pos = rand_idx % (in_cache.metas.size() + out_cache.metas.size());
    auto &meta = pos < in_cache.metas.size() ? in_cache.metas[pos] : out_cache.metas[pos - in_cache.metas.size()];
    meta.emplace_sample(current_seq);
}


void ThreeLCacheCache::update_stat_periodic() {
}


bool ThreeLCacheCache::lookup(const SimpleRequest &req) {
    bool ret;
    ++current_seq;
    if (is_full == 1)
        n_req++;
    auto it = key_map.find(req.id);
    if (it != key_map.end()) {
        auto list_idx = it->second.list_idx;
        auto list_pos = it->second.list_pos;
        if (is_full == 1) {
            if (list_idx == 0)
                n_hit++;
            n_window_hit++;
        }
        Meta &meta = list_idx == 0 ?  in_cache.metas[list_pos]: out_cache.metas[uint32_t(list_pos - out_cache.front_index)];
        auto sample_time = meta._sample_times;
        if (sample_time != 0 && (_distribution(_generator) % 4 == 0 || !booster)) {
            uint32_t future_distance = current_seq - sample_time;
            training_data->emplace_back(meta, sample_time, future_distance, meta._key);
            if (training_data->labels.size() >= batch_size && evict_nums <= 0) {
                train();
                training_data->clear();
            }
            meta._sample_times = 0;
        } else {
            meta._sample_times = 0;
        }

        meta.update(current_seq);
        if (!list_idx) { 
            if(samplepointer == list_pos){
                samplepointer = in_cache.dq[samplepointer].next;
            }
            if(pred_map.find(req.id) != pred_map.end()){
                pred_map.erase(req.id);
            }
            in_cache.re_request(list_pos);
        }
        ret = !list_idx;
    } else {
        ret = false;
    }
    
    if (is_sampling) {
        sample();
    }

    erase_out_cache();
    return ret;
}

void ThreeLCacheCache::erase_out_cache() {
    max_out_cache_size = in_cache.metas.size() * (hsw - 1) + 2;
    
    if (out_cache.metas.size() >= max_out_cache_size) {
        if (is_full == 0)
            is_full = 1;
        auto &meta = out_cache.metas[0];
        if (meta._size != 0) {
            auto sample_time = meta._sample_times;
           if (sample_time != 0 && (_distribution(_generator) % 4 == 0 || !booster)) {
                uint32_t future_distance = MAX_EVICTION_BOUNDARY[0] + current_seq - meta._past_timestamp;
                if (MAX_EVICTION_BOUNDARY[1] < current_seq - meta._past_timestamp)
                    MAX_EVICTION_BOUNDARY[1] = current_seq - meta._past_timestamp;
                    training_data->emplace_back(meta, sample_time, future_distance, meta._key);
                    
                if (training_data->labels.size() >= batch_size && evict_nums <= 0) {
                    train();
                    training_data->clear();
                }
            }
            key_map.erase(meta._key);
            meta.free();
       }
        out_cache.metas.pop_front();
        out_cache.front_index++;
    }
}

void ThreeLCacheCache::admit(const SimpleRequest &req) {
    const uint64_t &size = req.size;
    if (size > _cacheSize) {
        LOG("L", _cacheSize, req.id, size);
        return;
    }
    auto it = key_map.find(req.id);
    uint32_t pos;
    if (it == key_map.end()){

        pos = in_cache.metas.size();
        in_cache.metas.emplace_back(Meta(req.id, req.size, current_seq));
        in_cache.dq.emplace_back(CircleList());
        key_map[req.id] = {0, pos};
    } else {
        pos = in_cache.metas.size();
        in_cache.metas.emplace_back(out_cache.metas[uint32_t(it->second.list_pos - out_cache.front_index)]);
        out_cache.metas[uint32_t(it->second.list_pos - out_cache.front_index)]._size = 0;
        in_cache.dq.emplace_back(CircleList());
        in_cache.metas[pos]._size = size;
        key_map.find(req.id)->second = {0, pos};
    }  

    in_cache.request(pos);
    
    _currentSize += size;
    
    if (booster) {
        new_obj_size += req.size;
        new_obj_keys.emplace_back(req.id);
    }
}

uint32_t ThreeLCacheCache::rank() {
    vector<uint32_t> sampled_objects;
    if (initial_queue_length == 0) {
        initial_queue_length = in_cache.metas.size();
    }
    // 防止有trace在小缓存小只能存1-2个对象
    if (sample_rate >= initial_queue_length * 0.01 + eviction_rate)
        sample_rate = initial_queue_length > 2 ? initial_queue_length * 0.01 + eviction_rate : 1;
    // 新对象的采样
    sampled_objects = quick_demotion();

    if (new_obj_size < _currentSize * reserved_space / 10) {
        unsigned int idx_row = 0;
        uint16_t freq = 0;
        while (idx_row < sample_rate && sampled_objects.size() < initial_queue_length) {
            freq = in_cache.metas[samplepointer]._freq - 1;
            if (evcition_distribution[3] == 0 && scan_length > initial_queue_length * sampling_lru / 100){
                evcition_distribution[2] = evcition_distribution[0], evcition_distribution[3] = evcition_distribution[1];
                evcition_distribution[1] = 0, evcition_distribution[0] = 0;
            }
            if (freq  < sample_boundary || scan_length <= initial_queue_length * sampling_lru / 100 + eviction_rate) {
                sampled_objects.emplace_back(samplepointer);
                idx_row++;
            }

            scan_length++;
            
            if (scan_length >= initial_queue_length) {
                initial_queue_length = in_cache.metas.size();
                sample_rate = 1024;
                if (sample_rate >= initial_queue_length * 0.01 + eviction_rate) {
                    sample_rate = initial_queue_length > 2 ? initial_queue_length * 0.01 + eviction_rate : 1;
                }
                idx_row = 0;
                samplepointer = in_cache.q.head;
                scan_length = 0;
                pred_map.clear();
                pred_times.clear();
                pred_times.shrink_to_fit();
                if (objective == object_miss_ratio) {
                    continue;
                }
                uint32_t eviciton_sum = 0, p99 = 0;
                for (int i =  0; i < 16; i++)
                    eviciton_sum += object_distribution_n_eviction[i];
                // 粗粒度划分频率
                for (int i = 0; i < 16; i++) {  
                    p99 += object_distribution_n_eviction[i];
                    if (p99 >= 0.99 * eviciton_sum) {
                        if (i == 0)
                            sample_boundary = 1;
                        else                
                            sample_boundary = pow(2, i - 1) + ceil((pow(2, i) - pow(2, i - 1)) * ((0.99 * eviciton_sum + object_distribution_n_eviction[i] - p99) / object_distribution_n_eviction[i]));
                        break;
                    }
                }
                if (evcition_distribution[2] * evcition_distribution[1] > evcition_distribution[0] * evcition_distribution[3]) 
                    sampling_lru++;
                else if (sampling_lru > 1)
                    sampling_lru--;
                if ((evcition_distribution[0] + evcition_distribution[2]) > new_obj_keys.size())
                    reserved_space ++;
                else if (reserved_space > 1)
                    reserved_space /= 2;

                memset(evcition_distribution, 0, sizeof(uint64_t) * 4);
                memset(object_distribution_n_eviction, 0, sizeof(uint32_t) * 16);
                continue;
                
            }
            samplepointer = in_cache.dq[samplepointer].next;
        }
        spointer_timestamp = in_cache.metas[sampled_objects.back()]._past_timestamp;
        evcition_distribution[1] += sample_rate;
    }
    prediction(sampled_objects);
    return sampled_objects.size();
}

vector<uint32_t> ThreeLCacheCache::quick_demotion() {
    vector<uint32_t> sampled_objects;
    int i, j = 0;
    while (new_obj_size > _currentSize * reserved_space / 100  && j < sample_rate * 1.5 && i < new_obj_keys.size()) {
        auto it = key_map.find(new_obj_keys[i])->second;
        if (it.list_idx == 0) {
            new_obj_size -= in_cache.metas[it.list_pos]._size;
            sampled_objects.emplace_back(it.list_pos);
            j++;
        } else {
            new_obj_size -= out_cache.metas[it.list_pos - out_cache.front_index]._size;
        }
        i++;
    }
    new_obj_keys.erase(new_obj_keys.begin(), new_obj_keys.begin() + i);
    if (new_obj_keys.size() == 0)
        new_obj_size = 0;
    return sampled_objects;
}

void ThreeLCacheCache::evict() {
    auto epair = evict_predobj();
    evict_with_candidate(epair);
}

void ThreeLCacheCache::evict_with_candidate(pair<uint64_t, uint32_t> &epair) {
    is_sampling = true;
    evict_nums -= 1;
    uint64_t key = epair.first;
    uint32_t old_pos = epair.second;
    _currentSize -= in_cache.metas[old_pos]._size;

    pred_map.erase(key);
    if (old_pos == samplepointer){
        samplepointer = in_cache.dq[samplepointer].next;
    }
    key_map.find(key)->second = {1, uint32_t(out_cache.metas.size()) + out_cache.front_index};
    out_cache.metas.emplace_back(in_cache.metas[old_pos]);
    in_cache.erase(old_pos);
    uint32_t in_cache_tail_idx = in_cache.metas.size() - 1;
    if (old_pos != in_cache_tail_idx) {
        if (samplepointer == in_cache_tail_idx){
            samplepointer = old_pos;
        }
        in_cache.dq[in_cache.dq[in_cache_tail_idx].prev].next = old_pos;
        in_cache.dq[in_cache.dq[in_cache_tail_idx].next].prev = old_pos;

        key_map.find(in_cache.metas.back()._key)->second.list_pos = old_pos;
        in_cache.metas[old_pos] = in_cache.metas.back();
        in_cache.dq[old_pos] = in_cache.dq.back();
        if (in_cache.q.tail == in_cache_tail_idx)
            in_cache.q.tail = old_pos;
        if (in_cache.q.head == in_cache_tail_idx)
            in_cache.q.head = old_pos;
    }
    
    in_cache.metas.pop_back();
    in_cache.dq.pop_back();
}

pair<uint64_t, uint32_t> ThreeLCacheCache::evict_predobj(){
    {
        auto pos = in_cache.q.head;
        auto &meta = in_cache.metas[pos];
        if (!booster) {
            return {meta._key, pos};
        } 
    }
    if (evict_nums <= 0 || pred_map.empty()) {
        evict_nums = rank() / eviction_rate;
    }
    
    float reuse_time;
    uint64_t key;
    while (!pred_times.empty())
    {
        reuse_time = pred_times.front().reuse_time;
        key = pred_times.front().key;
        pop_heap(pred_times.begin(), pred_times.end(), 
        [](const HeapUint& a, const HeapUint& b) {
            return a.reuse_time < b.reuse_time;
        });
        pred_times.pop_back();
        if(pred_map.find(key) != pred_map.end() && pred_map[key] == reuse_time){
            uint32_t old_pos = key_map.find(key)->second.list_pos;
            object_distribution_n_eviction[uint16_t(log2(in_cache.metas[old_pos]._freq))]++;
            if (in_cache.metas[old_pos]._past_timestamp <= spointer_timestamp) 
                evcition_distribution[0]++;
            return {key, old_pos};
        }
    }

    return {-1, -1};
}


void ThreeLCacheCache::prediction(vector<uint32_t> sampled_objects) {
    uint32_t sample_nums = sampled_objects.size();
    int32_t indptr[sample_nums + 1];
    indptr[0] = 0;
    int32_t indices[sample_nums * n_feature];
    double data[sample_nums * n_feature];
    int32_t past_timestamps[sample_nums];
    uint32_t sizes[sample_nums];
    uint64_t keys[sample_nums];
    uint32_t poses[sample_nums];
    unsigned int idx_feature = 0;
    uint32_t pos;
    unsigned int idx_row = 0;
    for (; idx_row < sample_nums; idx_row++) {
        pos = sampled_objects[idx_row];
        auto &meta = in_cache.metas[pos];
        keys[idx_row] = meta._key;
        poses[idx_row] = pos;
        indices[idx_feature] = 0;
        // 年龄
        data[idx_feature++] = current_seq - meta._past_timestamp;
        
        past_timestamps[idx_row] = meta._past_timestamp;

        uint8_t j = 0;

        uint16_t n_within = meta._freq;
        if (meta._extra) {
            for (j = 0; j < meta._extra->_past_distance_idx && j < max_n_past_distances; ++j) {
                uint8_t past_distance_idx = (meta._extra->_past_distance_idx - 1 - j) % max_n_past_distances;
                uint32_t &past_distance = meta._extra->_past_distances[past_distance_idx];
                indices[idx_feature] = j + 1;
                data[idx_feature++] = past_distance;
            }
        }

        indices[idx_feature] = max_n_past_timestamps;
        data[idx_feature++] = meta._size;
        sizes[idx_row] = meta._size;

        indices[idx_feature] = max_n_past_timestamps + 1;
        data[idx_feature++] = n_within;

        indptr[idx_row + 1] = idx_feature;
    }
    int64_t len = 0;
    double scores[sample_nums];
    std::string inference_params_str;
    for (const auto &param : inference_params) {
        inference_params_str += param.first + "=" + param.second + " ";
    }
    inference_params_str.pop_back();  // Remove trailing space
    const char *inference_params_cstr = inference_params_str.c_str();
    LGBM_BoosterPredictForCSR(booster,
                              static_cast<void *>(indptr),
                              C_API_DTYPE_INT32,
                              indices,
                              static_cast<void *>(data),
                              C_API_DTYPE_FLOAT64,
                              idx_row + 1,
                              idx_feature,
                              n_feature,  //remove future t
                              C_API_PREDICT_NORMAL,
                              0,
                              0,
                              inference_params_cstr,
                              &len,
                              scores);
    float _distance;
    if (objective == byte_miss_ratio) {
        for (int i = 0; i < sample_nums; ++i) {
            _distance = exp(scores[i]) + uint64_t(current_seq - origin_current_seq);
            pred_times.push_back({_distance, keys[i]});
            push_heap(pred_times.begin(), pred_times.end(), 
            [](const HeapUint& a, const HeapUint& b) {
                return a.reuse_time < b.reuse_time;
            });
            pred_map[keys[i]] = _distance;
        }
    } else {
        for (int i = 0; i < sample_nums; ++i) {
            _distance = float(sizes[i] * exp(scores[i]));
            pred_times.push_back({_distance, keys[i]});
            push_heap(pred_times.begin(), pred_times.end(), 
            [](const HeapUint& a, const HeapUint& b) {
                return a.reuse_time < b.reuse_time;
            });
            pred_map[keys[i]] = _distance;
        }
    }
}

