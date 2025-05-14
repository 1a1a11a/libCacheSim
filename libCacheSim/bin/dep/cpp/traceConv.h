#pragma once

#include <iostream>
#include <string>

#include "common.h"

typedef void(parse_func_t)(std::string line, struct trace_req *req);
