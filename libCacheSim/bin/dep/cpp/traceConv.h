#pragma once 

#include <string> 
#include <iostream>

#include "common.h"


typedef void(parse_func_t)(std::string line, struct trace_req *req);
