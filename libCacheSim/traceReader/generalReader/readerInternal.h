#pragma once

#include "../../include/libCacheSim/reader.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_LINE_LEN (1024 * 8)

#define MAX_OBJ_ID_LEN 1024


bool find_end_of_line(reader_t *reader, char **next_line, size_t *line_len);

#ifdef __cplusplus
}
#endif
