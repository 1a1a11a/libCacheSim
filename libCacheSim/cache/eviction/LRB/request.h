#ifndef REQUEST_H
#define REQUEST_H

#include <cstdint>
#include <iostream>
#include <vector>

using namespace std;

// Request information
class SimpleRequest {
public:
    SimpleRequest() = default;

    virtual ~SimpleRequest() = default;

    SimpleRequest(
            const int64_t &_id,
            const int64_t &_size) {
        reinit(0, _id, _size);
    }

    SimpleRequest(const int64_t &_seq, const int64_t &_id, const int64_t &_size, const vector<uint16_t> *_extra_features = nullptr) {
        reinit(_seq, _id, _size, _extra_features);
    }

    void reinit(const int64_t &_seq, const int64_t &_id, const int64_t &_size, const vector<uint16_t> *_extra_features = nullptr) {
        seq = _seq;
        id = _id;
        size = _size;
        if (_extra_features) {
            extra_features = *_extra_features;
        }
    }

    uint64_t seq{};
    int64_t id{}; // request object id
    int64_t size{}; // request size in bytes
    //category feature. unsigned int. ideally not exceed 2k
    vector<uint16_t > extra_features;
};


class AnnotatedRequest : public SimpleRequest {
public:
    AnnotatedRequest() = default;

    // Create request
    AnnotatedRequest(const int64_t &_seq, const int64_t &_id, const int64_t &_size, const int64_t &_next_seq,
                     const vector<uint16_t> *_extra_features = nullptr)
            : SimpleRequest(_seq, _id, _size, _extra_features),
              next_seq(_next_seq) {}

    void reinit(const int64_t &_seq, const int64_t &_id, const int64_t &_size, const int64_t &_next_seq,
                       const vector<uint16_t> *_extra_features = nullptr) {
        SimpleRequest::reinit(_seq, _id, _size, _extra_features);
        next_seq = _next_seq;
    }

    int64_t next_seq{};
};


#endif /* REQUEST_H */


