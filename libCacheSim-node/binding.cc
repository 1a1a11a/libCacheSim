/**
 * @file binding.cc
 * @brief Implements the N-API bindings for libCacheSim.
 *
 * This file creates a native Node.js addon that exposes the core cache
 * simulation functionality of the libCacheSim library to JavaScript.
 */

#include <napi.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "libCacheSim.h"

// Helper function to check if a file exists.
bool fileExists(const std::string& filename) {
  struct stat buffer;
  return (stat(filename.c_str(), &buffer) == 0);
}

// Helper function to parse a cache size string (e.g., "1mb", "1gb", "1024")
// into a uint64_t byte value.
uint64_t parseCacheSize(const std::string& sizeStr) {
  if (sizeStr.empty()) return 0;

  std::string lower = sizeStr;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

  size_t pos = 0;
  while (pos < lower.length() && (isdigit(lower[pos]) || lower[pos] == '.')) {
    pos++;
  }

  double value = std::stod(lower.substr(0, pos));
  std::string unit = lower.substr(pos);

  uint64_t multiplier = 1;
  if (unit == "kb" || unit == "k")
    multiplier = 1024;
  else if (unit == "mb" || unit == "m")
    multiplier = 1024 * 1024;
  else if (unit == "gb" || unit == "g")
    multiplier = 1024 * 1024 * 1024;
  else if (unit == "tb" || unit == "t")
    multiplier = 1024ULL * 1024 * 1024 * 1024;

  return (uint64_t)(value * multiplier);
}

// Helper function to get a cache constructor by algorithm name.
cache_t* createCache(const std::string& algo,
                     const common_cache_params_t& params) {
  std::string lowerAlgo = algo;
  std::transform(lowerAlgo.begin(), lowerAlgo.end(), lowerAlgo.begin(),
                 ::tolower);

  if (lowerAlgo == "lru") return LRU_init(params, nullptr);
  if (lowerAlgo == "fifo") return FIFO_init(params, nullptr);
  if (lowerAlgo == "lfu") return LFU_init(params, nullptr);
  if (lowerAlgo == "arc") return ARC_init(params, nullptr);
  if (lowerAlgo == "clock") return Clock_init(params, nullptr);
  if (lowerAlgo == "s3fifo") return S3FIFO_init(params, nullptr);
  if (lowerAlgo == "sieve") return Sieve_init(params, nullptr);

  return nullptr;  // Unknown algorithm
}

/**
 * @brief Runs a cache simulation with specified parameters.
 *
 * This function is exposed to JavaScript. It takes the trace path, trace type,
 * algorithm, and an optional cache size, runs the simulation, and returns an
 * object with the results.
 *
 * @param info N-API callback info.
 *   - arg 0 (String): Path to the trace file.
 *   - arg 1 (String): Type of the trace (e.g., "vscsi", "csv", "oracle").
 *   - arg 2 (String): Caching algorithm to use (e.g., "lru", "s3fifo").
 *   - arg 3 (String, optional): Cache size (e.g., "1MB", "256gb"). Defaults to "1MB".
 * @return Napi::Value An object containing simulation statistics (totalRequests,
 *   hits, misses, hitRatio, missRatio, etc.).
 */
Napi::Value runSimulation(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 3) {
    Napi::TypeError::New(env, "Expected 3-4 arguments: tracePath, traceType, algorithm, [cacheSize]")
        .ThrowAsJavaScriptException();
    return env.Null();
  }
  if (!info[0].IsString() || !info[1].IsString() || !info[2].IsString()) {
    Napi::TypeError::New(env, "First three arguments must be strings").ThrowAsJavaScriptException();
    return env.Null();
  }

  std::string tracePath = info[0].As<Napi::String>().Utf8Value();
  std::string traceType = info[1].As<Napi::String>().Utf8Value();
  std::string algorithm = info[2].As<Napi::String>().Utf8Value();

  if (!fileExists(tracePath)) {
    Napi::Error::New(env, "Trace file does not exist: " + tracePath).ThrowAsJavaScriptException();
    return env.Null();
  }

  uint64_t cacheSize = 1024 * 1024;  // 1MB default
  if (info.Length() > 3 && info[3].IsString()) {
    cacheSize = parseCacheSize(info[3].As<Napi::String>().Utf8Value());
  }

  trace_type_e trace_type_enum;
  std::string lowerTraceType = traceType;
  std::transform(lowerTraceType.begin(), lowerTraceType.end(), lowerTraceType.begin(), ::tolower);
  if (lowerTraceType == "vscsi") trace_type_enum = VSCSI_TRACE;
  else if (lowerTraceType == "csv") trace_type_enum = CSV_TRACE;
  else if (lowerTraceType == "txt") trace_type_enum = PLAIN_TXT_TRACE;
  else if (lowerTraceType == "binary") trace_type_enum = BIN_TRACE;
  else if (lowerTraceType == "oracle") trace_type_enum = ORACLE_GENERAL_TRACE;
  else {
    Napi::Error::New(env, "Unsupported trace type.").ThrowAsJavaScriptException();
    return env.Null();
  }

  common_cache_params_t cc_params = {.cache_size = cacheSize, .default_ttl = 0, .hashpower = 24, .consider_obj_metadata = false};
  cache_t* cache = createCache(algorithm, cc_params);
  if (!cache) {
    Napi::Error::New(env, "Failed to create cache. Unsupported algorithm?").ThrowAsJavaScriptException();
    return env.Null();
  }

  reader_t* reader = open_trace(tracePath.c_str(), trace_type_enum, nullptr);
  if (!reader) {
    cache->cache_free(cache);
    Napi::Error::New(env, "Failed to open trace file.").ThrowAsJavaScriptException();
    return env.Null();
  }

  request_t* req = new_request();
  uint64_t n_req = 0, n_miss = 0;
  while (read_one_req(reader, req) == 0) {
    if (!cache->get(cache, req)) n_miss++;
    n_req++;
  }

  close_trace(reader);
  free_request(req);
  cache->cache_free(cache);

  Napi::Object result = Napi::Object::New(env);
  result.Set("totalRequests", Napi::Number::New(env, n_req));
  result.Set("hits", Napi::Number::New(env, n_req - n_miss));
  result.Set("misses", Napi::Number::New(env, n_miss));
  result.Set("hitRatio", n_req > 0 ? Napi::Number::New(env, (double)(n_req - n_miss) / n_req) : Napi::Number::New(env, 0.0));
  result.Set("missRatio", n_req > 0 ? Napi::Number::New(env, (double)n_miss / n_req) : Napi::Number::New(env, 0.0));
  result.Set("algorithm", Napi::String::New(env, algorithm));
  result.Set("cacheSize", Napi::String::New(env, info.Length() > 3 ? info[3].As<Napi::String>().Utf8Value() : "1MB"));

  return result;
}

/**
 * @brief Runs a simple, hardcoded simulation for basic testing.
 *
 * This function is exposed for backward compatibility and simple tests. It runs
 * an LRU simulation with a 1MB cache on a default trace file.
 *
 * @param info N-API callback info (not used).
 * @return Napi::Value An object containing simulation statistics.
 */
Napi::Value runSim(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  const char* default_trace = "../data/cloudPhysicsIO.vscsi";

  if (!fileExists(default_trace)) {
    Napi::Error::New(env, "Default trace file not found: " + std::string(default_trace)).ThrowAsJavaScriptException();
    return env.Null();
  }

  reader_t* reader = open_trace(default_trace, VSCSI_TRACE, nullptr);
  if (!reader) {
    Napi::Error::New(env, "Failed to open trace").ThrowAsJavaScriptException();
    return env.Null();
  }

  request_t* req = new_request();
  common_cache_params_t cc_params = {.cache_size = 1024 * 1024, .default_ttl = 0, .hashpower = 24, .consider_obj_metadata = false};
  cache_t* cache = LRU_init(cc_params, nullptr);
  if (!cache) {
    close_trace(reader);
    free_request(req);
    Napi::Error::New(env, "Failed to create cache").ThrowAsJavaScriptException();
    return env.Null();
  }

  uint64_t n_req = 0, n_miss = 0;
  while (read_one_req(reader, req) == 0) {
    if (!cache->get(cache, req)) n_miss++;
    n_req++;
  }

  close_trace(reader);
  free_request(req);
  cache->cache_free(cache);

  Napi::Object result = Napi::Object::New(env);
  result.Set("totalRequests", Napi::Number::New(env, n_req));
  result.Set("hits", Napi::Number::New(env, n_req - n_miss));
  result.Set("misses", Napi::Number::New(env, n_miss));
  result.Set("hitRatio", n_req > 0 ? Napi::Number::New(env, (double)(n_req - n_miss) / n_req) : Napi::Number::New(env, 0.0));
  result.Set("missRatio", n_req > 0 ? Napi::Number::New(env, (double)n_miss / n_req) : Napi::Number::New(env, 0.0));
  result.Set("algorithm", Napi::String::New(env, "lru"));
  result.Set("cacheSize", Napi::String::New(env, "1MB"));

  return result;
}

// Initializes the Node.js addon, exporting the wrapped functions.
Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set("runSim", Napi::Function::New(env, runSim));
  exports.Set("runSimulation", Napi::Function::New(env, runSimulation));
  return exports;
}

NODE_API_MODULE(libcachesim_addon, Init)
