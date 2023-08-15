#pragma once

#include <time.h>

#include "traceConv.h"

static inline void parse_tencent_block(std::string line,
                                       struct trace_req *req) {
    // # 1538323199,105352008,584,1,1576

    uint64_t lba;

    char *p = strtok(line.data(), ",");
    req->ts = (uint32_t) atoi(p);
    p = strtok(NULL, ",");
    lba = atoll(p);
    p = strtok(NULL, ",");
    req->sz = atoi(p) * 512;

    p = strtok(NULL, ",");
    if (*p == '0') {
        req->op = 1;
    } else if (*p == '1') {
        req->op = 3;
    } else {
        std::cerr << "unknown op: " << *p << std::endl;
        req->op = 0;
    }

    p = strtok(NULL, ",");
    req->ns = (uint16_t) atoi(p);

    req->obj_id = lba + req->ns;
    p = strtok(NULL, ",");
    assert(p == NULL);
}

static inline void parse_tencent_photo(std::string line,
                                       struct trace_req *req) {
    // 20160201164559 001488a9e71a3064716358a6e62a99344068d7bd 0 m 80234 1 PHONE 0
    // ts, obj_id, _, _, sz, _, _, _ = line.strip().split()

    struct tm tm = {};

    char *p = strtok(line.data(), " ");

    strptime(p, "%Y%m%d%H%M%S", &tm);
    req->ts = (uint32_t) std::mktime(&tm);

    p = strtok(NULL, " ");
    req->obj_id = (int64_t) std::hash<std::string>{}(p);

    p = strtok(NULL, " ");
    p = strtok(NULL, " ");
    p = strtok(NULL, " ");
    req->sz = atol(p);

    p = strtok(NULL, " ");
    p = strtok(NULL, " ");
    p = strtok(NULL, " ");
    p = strtok(NULL, " ");
    assert(p == NULL);
}