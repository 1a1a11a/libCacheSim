

//  This module is used to replace the old consistent hashing module as we don't need it now
//
//  hasher.hpp
//  CDNSimulator
//
//  Created by Juncheng on 05/08/19.
//  Copyright © 2019 Juncheng. All rights reserved.
//

#ifndef HASHER_HPP
#define HASHER_HPP

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include "logging.h"
#include "md5.h"

#ifdef __cplusplus
}
#endif

#include <atomic>
#include <string>
#include <iostream>
#include <vector>
#include <sstream>
//#include <cstring>

namespace CDNSimulator {
    class myHasher{
    public:
        unsigned int n;
        explicit myHasher(unsigned int n):n(n){};

        unsigned int get_idx(request_t* cp){
            unsigned int idx;
            get_k_idx(cp, 1, &idx);
            return idx;
        }

        void get_k_idx(request_t* cp, unsigned int k, unsigned int* idx){
            if (cp->obj_id_type == OBJ_ID_STR);
            else if (cp->obj_id_type == OBJ_ID_NUM) {
                cp->label[8] = 0;
            } else {
                ERROR("unknown request_t datatype %c\n", cp->obj_id_type);
                abort();
            }

            if (k >= 60){
                ERROR("only support fewer than 60 idx");
                abort();
            }

            md5_state_t md5state;
            unsigned char digest[16];

            md5_init(&md5state);
            md5_append(&md5state, (unsigned char *) (cp->label_ptr), (int) strlen((char*) cp->label_ptr));
            md5_finish(&md5state, digest);

            unsigned int first_idx = (digest[3u] << 24u | (digest[2] << 16u) | (digest[1] << 8u) | digest[0u]) % n;
            for (unsigned int i=0; i<k; i++){
                idx[i] = (first_idx + i) % n;
            }
        }
    };

} // namespace CDNSimulator

#endif /* HASHER_HPP */
