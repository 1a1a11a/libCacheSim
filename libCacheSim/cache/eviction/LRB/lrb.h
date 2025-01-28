//
// Created by zhenyus on 1/16/19.
//

#ifndef WEBCACHESIM_LRB_H
#define WEBCACHESIM_LRB_H

#include "cache.h"
#include <unordered_map>
#include <unordered_set>
#include "../../../dataStructure/sparsepp/spp.h"
#include <vector>
#include <random>
#include <cmath>
#include <LightGBM/c_api.h>
#include <assert.h>
#include <sstream>
#include <fstream>
#include <list>

using namespace webcachesim;
using namespace std;

using spp::sparse_hash_map;

namespace lrb {
    static const uint8_t n_edc_feature = 10;
    static const uint max_n_extra_feature = 4;
    static const uint32_t n_extra_fields = 0;
    static const uint8_t max_n_past_timestamps = 32;
    static const uint8_t max_n_past_distances = 31;
    static const uint8_t base_edc_window = 10;
    static const uint32_t batch_size = 131072;


struct MetaExtra {
    //vector overhead = 24 (8 pointer, 8 size, 8 allocation)
    //40 + 24 + 4*x + 1
    //65 byte at least
    //189 byte at most
    //not 1 hit wonder
    float _edc[10];
    vector<uint32_t> _past_distances;
    //the next index to put the distance
    uint8_t _past_distance_idx = 1;

    MetaExtra(const uint32_t &distance,
    uint32_t max_hash_edc_idx,
    vector<uint32_t> &edc_windows,
    const vector<double> &hash_edc
    ) {
        _past_distances = vector<uint32_t>(1, distance);
        for (uint8_t i = 0; i < n_edc_feature; ++i) {
            uint32_t _distance_idx = min(uint32_t(distance / edc_windows[i]), max_hash_edc_idx);
            _edc[i] = hash_edc[_distance_idx] + 1;
        }
    }

    void update(const uint32_t &distance,
        uint32_t max_hash_edc_idx,
        vector<uint32_t> &edc_windows,
        const vector<double> &hash_edc
    ) {
        uint8_t distance_idx = _past_distance_idx % max_n_past_distances;
        if (_past_distances.size() < max_n_past_distances)
            _past_distances.emplace_back(distance);
        else
            _past_distances[distance_idx] = distance;
        assert(_past_distances.size() <= max_n_past_distances);
        _past_distance_idx = _past_distance_idx + (uint8_t) 1;
        if (_past_distance_idx >= max_n_past_distances * 2)
            _past_distance_idx -= max_n_past_distances;
        for (uint8_t i = 0; i < n_edc_feature; ++i) {
            uint32_t _distance_idx = min(uint32_t(distance / edc_windows[i]), max_hash_edc_idx);
            _edc[i] = _edc[i] * hash_edc[_distance_idx] + 1;
        }
    }
};

class Meta {
public:
    //25 byte
    uint64_t _key;
    uint64_t _size;
    uint32_t _past_timestamp;
    uint16_t _extra_features[max_n_extra_feature];
    MetaExtra *_extra = nullptr;
    vector<uint32_t> _sample_times;

    Meta(const uint64_t &key, const uint64_t &size, const uint64_t &past_timestamp,
            const vector<uint16_t> &extra_features) {
        _key = key;
        _size = size;
        _past_timestamp = past_timestamp;
        for (int i = 0; i < n_extra_fields; ++i)
            _extra_features[i] = extra_features[i];
    }

    virtual ~Meta() = default;

    void emplace_sample(uint32_t &sample_t) {
        _sample_times.emplace_back(sample_t);
    }

    void free() {
        delete _extra;
    }

    void update(const uint32_t &past_timestamp, uint32_t n_extra_fields,
                uint32_t max_hash_edc_idx,
                vector<uint32_t> &edc_windows,
                const vector<double> &hash_edc
    ) {
        //distance
        uint32_t _distance = past_timestamp - _past_timestamp;
        assert(_distance);
        if (!_extra) {
            _extra = new MetaExtra(_distance, max_hash_edc_idx, edc_windows, hash_edc);
        } else
            _extra->update(_distance, max_hash_edc_idx, edc_windows, hash_edc);
        //timestamp
        _past_timestamp = past_timestamp;
    }

    int feature_overhead() {
        int ret = sizeof(Meta);
        if (_extra)
            ret += sizeof(MetaExtra) - sizeof(_sample_times) + _extra->_past_distances.capacity() * sizeof(uint32_t);
        return ret;
    }

    int sample_overhead() {
        return sizeof(_sample_times) + sizeof(uint32_t) * _sample_times.capacity();
    }
};


class InCacheMeta : public Meta {
public:
    //pointer to lru0
    list<int64_t>::const_iterator p_last_request;
    //any change to functions?

    InCacheMeta(const uint64_t &key,
                const uint64_t &size,
                const uint64_t &past_timestamp,
                const vector<uint16_t> &extra_features, const list<int64_t>::const_iterator &it) :
            Meta(key, size, past_timestamp, extra_features) {
        p_last_request = it;
    };

    InCacheMeta(const Meta &meta, const list<int64_t>::const_iterator &it) : Meta(meta) {
        p_last_request = it;
    };

};

class InCacheLRUQueue {
public:
    list<int64_t> dq;

    //the hashtable (location information is maintained outside, and assume it is always correct)
    list<int64_t>::const_iterator request(int64_t key) {
        dq.emplace_front(key);
        return dq.cbegin();
    }

    list<int64_t>::const_iterator re_request(list<int64_t>::const_iterator it) {
        if (it != dq.cbegin()) {
            dq.emplace_front(*it);
            dq.erase(it);
        }
        return dq.cbegin();
    }
};

class TrainingData {
public:
    vector<float> labels;
    vector<int32_t> indptr;
    vector<int32_t> indices;
    vector<double> data;
    uint32_t memory_window;

    TrainingData(uint32_t n_feature, uint32_t memory_window_) {
        labels.reserve(batch_size);
        indptr.reserve(batch_size + 1);
        indptr.emplace_back(0);
        indices.reserve(batch_size * n_feature);
        data.reserve(batch_size * n_feature);
        memory_window = memory_window_;
    }

    void emplace_back(Meta &meta, uint32_t &sample_timestamp, uint32_t &future_interval, const uint64_t &key,
        uint32_t max_hash_edc_idx, vector<uint32_t> &edc_windows, vector<double> &hash_edc) {
        int32_t counter = indptr.back();

        indices.emplace_back(0);
        data.emplace_back(sample_timestamp - meta._past_timestamp);
        ++counter;

        uint32_t this_past_distance = 0;
        int j = 0;
        uint8_t n_within = 0;
        if (meta._extra) {
            for (; j < meta._extra->_past_distance_idx && j < max_n_past_distances; ++j) {
                uint8_t past_distance_idx = (meta._extra->_past_distance_idx - 1 - j) % max_n_past_distances;
                const uint32_t &past_distance = meta._extra->_past_distances[past_distance_idx];
                this_past_distance += past_distance;
                indices.emplace_back(j + 1);
                data.emplace_back(past_distance);
                if (this_past_distance < memory_window) {
                    ++n_within;
                }
            }
        }

        counter += j;

        indices.emplace_back(max_n_past_timestamps);
        data.push_back(meta._size);
        ++counter;

        for (int k = 0; k < n_extra_fields; ++k) {
            indices.push_back(max_n_past_timestamps + k + 1);
            data.push_back(meta._extra_features[k]);
        }
        counter += n_extra_fields;

        indices.push_back(max_n_past_timestamps + n_extra_fields + 1);
        data.push_back(n_within);
        ++counter;

        if (meta._extra) {
            for (int k = 0; k < n_edc_feature; ++k) {
                indices.push_back(max_n_past_timestamps + n_extra_fields + 2 + k);
                uint32_t _distance_idx = std::min(
                        uint32_t(sample_timestamp - meta._past_timestamp) / edc_windows[k],
                        max_hash_edc_idx);
                data.push_back(meta._extra->_edc[k] * hash_edc[_distance_idx]);
            }
        } else {
            for (int k = 0; k < n_edc_feature; ++k) {
                indices.push_back(max_n_past_timestamps + n_extra_fields + 2 + k);
                uint32_t _distance_idx = std::min(
                        uint32_t(sample_timestamp - meta._past_timestamp) / edc_windows[k],
                        max_hash_edc_idx);
                data.push_back(hash_edc[_distance_idx]);
            }
        }

        counter += n_edc_feature;

        labels.push_back(log1p(future_interval));
        indptr.push_back(counter);
    }

    void clear() {
        labels.clear();
        indptr.resize(1);
        indices.clear();
        data.clear();
    }
};


struct KeyMapEntryT {
    unsigned int list_idx: 1;
    unsigned int list_pos: 31;
};

class LRBCache : public Cache {
public:
    uint32_t current_seq = -1;
    vector<uint32_t> edc_windows;
    vector<double> hash_edc;
    uint32_t max_hash_edc_idx;
    uint32_t memory_window = 67108864;
    uint32_t n_feature;

    //key -> (0/1 list, idx)
    sparse_hash_map<uint64_t, KeyMapEntryT> key_map;
    vector<InCacheMeta> in_cache_metas;
    vector<Meta> out_cache_metas;

    InCacheLRUQueue in_cache_lru_queue;
    shared_ptr<sparse_hash_map<uint64_t, uint64_t>> negative_candidate_queue;
    TrainingData *training_data;

    // sample_size: use n_memorize keys + random choose (sample_rate - n_memorize) keys
    uint sample_rate = 64;

    double training_loss = 0;
    int32_t n_force_eviction = 0;

    double training_time = 0;
    double inference_time = 0;

    BoosterHandle booster = nullptr;

    unordered_map<string, string> training_params = {
            //don't use alias here. C api may not recognize
            {"boosting",         "gbdt"},
            {"objective",        "regression"},
            {"num_iterations",   "32"},
            {"num_leaves",       "32"},
            {"num_threads",      "1"},
            {"feature_fraction", "0.8"},
            {"bagging_freq",     "5"},
            {"bagging_fraction", "0.8"},
            {"learning_rate",    "0.1"},
            {"verbosity",        "0"},
            {"force_row_wise", "true"},
    };

    unordered_map<string, string> inference_params;

    enum ObjectiveT : uint8_t {
        byte_miss_ratio = 0, object_miss_ratio = 1
    };
    ObjectiveT objective = object_miss_ratio;

    default_random_engine _generator = default_random_engine();
    uniform_int_distribution<std::size_t> _distribution = uniform_int_distribution<std::size_t>();

    vector<int> segment_n_in;
    vector<int> segment_n_out;
    uint32_t obj_distribution[2];
    uint32_t training_data_distribution[2];  //1: pos, 0: neg
    vector<float> segment_positive_example_ratio;
    vector<double> segment_percent_beyond;
    int n_retrain = 0;
    vector<int> segment_n_retrain;
    bool is_sampling = false;

    uint64_t byte_million_req;

    void init_with_params(const map<string, string> &params) override {
        //set params
        for (auto &it: params) {
            if (it.first == "sample_rate") {
                sample_rate = stoul(it.second);
            } else if (it.first == "memory_window") {
                memory_window = stoull(it.second);
            } else if (it.first == "num_iterations") {
                training_params["num_iterations"] = it.second;
            } else if (it.first == "learning_rate") {
                training_params["learning_rate"] = it.second;
            } else if (it.first == "num_threads") {
                training_params["num_threads"] = it.second;
            } else if (it.first == "num_leaves") {
                training_params["num_leaves"] = it.second;
            } else if (it.first == "byte_million_req") {
                byte_million_req = stoull(it.second);
            } else if (it.first == "n_edc_feature") {
                if (stoull(it.second) != n_edc_feature) {
                    cerr << "error: cannot change n_edc_feature because of const" << endl;
                    abort();
                }
//                n_edc_feature = stoull(it.second);
            } else if (it.first == "objective") {
                if (it.second == "byte-miss-ratio")
                    objective = byte_miss_ratio;
                else if (it.second == "object-miss-ratio")
                    objective = object_miss_ratio;
                else {
                    cerr << "error: unknown objective" << endl;
                    exit(-1);
                }
            } else {
                cerr << "LRB unrecognized parameter: " << it.first << endl;
            }
        }

        negative_candidate_queue = make_shared<sparse_hash_map<uint64_t, uint64_t>>(memory_window);
        // max_n_past_distances = max_n_past_timestamps - 1;

        //init
        edc_windows = vector<uint32_t>(n_edc_feature);
        for (uint8_t i = 0; i < n_edc_feature; ++i) {
            edc_windows[i] = pow(2, base_edc_window + i);
        }
        set_hash_edc();

        //interval, distances, size, extra_features, n_past_intervals, edwt
        n_feature = max_n_past_timestamps + n_extra_fields + 2 + n_edc_feature;
        if (n_extra_fields) {
            if (n_extra_fields > max_n_extra_feature) {
                cerr << "error: only support <= " + to_string(max_n_extra_feature)
                        + " extra fields because of static allocation" << endl;
                abort();
            }
            string categorical_feature = to_string(max_n_past_timestamps + 1);
            for (uint i = 0; i < n_extra_fields - 1; ++i) {
                categorical_feature += "," + to_string(max_n_past_timestamps + 2 + i);
            }
            training_params["categorical_feature"] = categorical_feature;
        }
        inference_params = training_params;
        training_data = new TrainingData(n_feature, memory_window);
    }

    string map_to_string(unordered_map<string, string> &map) {
        stringstream ss;
        for (auto &it: map) {
            ss << it.first << "=" << it.second << " ";
        }
        
        return ss.str();
    }

    bool lookup(const SimpleRequest &req) override;

    void admit(const SimpleRequest &req) override;

    void evict();

    void evict_with_candidate(pair<uint64_t, uint32_t> &epair);

    void forget();

    //sample, rank the 1st and return
    pair<uint64_t, uint32_t> rank();

    void train();

    void sample();

    void update_stat_periodic() override;

    void set_hash_edc() {
        max_hash_edc_idx = (uint64_t) (memory_window / pow(2, base_edc_window)) - 1;
        hash_edc = vector<double>(max_hash_edc_idx + 1);
        for (int i = 0; i < hash_edc.size(); ++i)
            hash_edc[i] = pow(0.5, i);
    }

    void remove_from_outcache_metas(Meta &meta, unsigned int &pos, const uint64_t &key);

    bool has(const uint64_t &id) {
        auto it = key_map.find(id);
        if (it == key_map.end())
            return false;
        return !it->second.list_idx;
    }

    vector<int> get_object_distribution_n_past_timestamps() {
        vector<int> distribution(max_n_past_timestamps, 0);
        for (auto &meta: in_cache_metas) {
            if (nullptr == meta._extra) {
                ++distribution[0];
            } else {
                ++distribution[meta._extra->_past_distances.size()];
            }
        }
        for (auto &meta: out_cache_metas) {
            if (nullptr == meta._extra) {
                ++distribution[0];
            } else {
                ++distribution[meta._extra->_past_distances.size()];
            }
        }
        return distribution;
    }

};

// static Factory<LRBCache> factoryLRB("LRB");

}
#endif //WEBCACHESIM_LRB_H