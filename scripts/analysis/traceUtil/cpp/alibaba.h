#pragma once 

#include "traceConv.h"

static inline void parse_alibaba(std::string line, struct trace_req *req) {
    // ns, op, lba, sz, ts = line.strip().split(",")
    uint64_t lba;

    char *p = strtok(line.data(), ",");
    req->ns = (uint16_t) atoi(p);

    p = strtok(NULL, ",");
    switch (*p) {
        case 'R':
            req->op = 1;
            break;
        case 'W':
            req->op = 3;
            break;
        default:
            std::cerr << "unknown op: " << *p << std::endl;
            req->op = 0;
            break;
    }

    p = strtok(NULL, ",");
    lba = atoll(p);
    req->obj_id = lba + req->ns;
    p = strtok(NULL, ",");
    req->sz = atoi(p);
    p = strtok(NULL, ",");
    req->ts = atoll(p) / 1000000;
    p = strtok(NULL, ",");
    assert(p == NULL);
}

