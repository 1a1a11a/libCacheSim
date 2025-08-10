#include <napi.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "libCacheSim.h"

// Helper function to check if file exists
bool fileExists(const std::string& filename) {
  struct stat buffer;
  return (stat(filename.c_str(), &buffer) == 0);
}

// Helper function to parse cache size string (e.g., "1mb", "1gb", "1024")
uint64_t parseCacheSize(const std::string& sizeStr) {
  if (sizeStr.empty()) return 0;

  std::string lower = sizeStr;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

  // Extract number and unit
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

// Helper function to get cache constructor by algorithm name
cache_t* createCache(const std::string& algo,
                     const common_cache_params_t& params) {
  std::string lowerAlgo = algo;
  std::transform(lowerAlgo.begin(), lowerAlgo.end(), lowerAlgo.begin(),
                 ::tolower);

  if (lowerAlgo == "lru")
    return LRU_init(params, nullptr);
  else if (lowerAlgo == "fifo")
    return FIFO_init(params, nullptr);
  else if (lowerAlgo == "lfu")
    return LFU_init(params, nullptr);
  else if (lowerAlgo == "arc")
    return ARC_init(params, nullptr);
  else if (lowerAlgo == "clock")
    return Clock_init(params, nullptr);
  else if (lowerAlgo == "s3fifo")
    return S3FIFO_init(params, nullptr);
  else if (lowerAlgo == "sieve")
    return Sieve_init(params, nullptr);

  return nullptr;  // Unknown algorithm
}

// Main simulation function
Napi::Value runSimulation(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // Check arguments
  if (info.Length() < 3) {
    Napi::TypeError::New(
        env, "Expected at least 3 arguments: tracePath, traceType, algorithm")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  if (!info[0].IsString() || !info[1].IsString() || !info[2].IsString()) {
    Napi::TypeError::New(env, "First three arguments must be strings")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  std::string tracePath = info[0].As<Napi::String>().Utf8Value();
  std::string traceType = info[1].As<Napi::String>().Utf8Value();
  std::string algorithm = info[2].As<Napi::String>().Utf8Value();

  // Check if file exists before trying to open it
  if (!fileExists(tracePath)) {
    Napi::Error::New(env, "Trace file does not exist: " + tracePath)
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  // Parse optional cache size (default 1MB)
  uint64_t cacheSize = 1024 * 1024;  // 1MB default
  if (info.Length() > 3 && info[3].IsString()) {
    try {
      cacheSize = parseCacheSize(info[3].As<Napi::String>().Utf8Value());
      if (cacheSize == 0) {
        Napi::Error::New(env, "Invalid cache size")
            .ThrowAsJavaScriptException();
        return env.Null();
      }
    } catch (const std::exception& e) {
      Napi::Error::New(env, "Invalid cache size format")
          .ThrowAsJavaScriptException();
      return env.Null();
    }
  }

  // Determine trace type enum
  trace_type_e trace_type_enum;
  std::string lowerTraceType = traceType;
  std::transform(lowerTraceType.begin(), lowerTraceType.end(),
                 lowerTraceType.begin(), ::tolower);

  if (lowerTraceType == "vscsi")
    trace_type_enum = VSCSI_TRACE;
  else if (lowerTraceType == "csv")
    trace_type_enum = CSV_TRACE;
  else if (lowerTraceType == "txt" || lowerTraceType == "plain_txt")
    trace_type_enum = PLAIN_TXT_TRACE;
  else if (lowerTraceType == "binary" || lowerTraceType == "bin")
    trace_type_enum = BIN_TRACE;
  else if (lowerTraceType == "oracle")
    trace_type_enum = ORACLE_GENERAL_TRACE;
  else {
    Napi::Error::New(
        env,
        "Unsupported trace type. Supported: vscsi, csv, txt, binary, oracle")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  // Validate algorithm before creating cache
  std::string lowerAlgo = algorithm;
  std::transform(lowerAlgo.begin(), lowerAlgo.end(), lowerAlgo.begin(),
                 ::tolower);
  if (lowerAlgo != "lru" && lowerAlgo != "fifo" && lowerAlgo != "lfu" &&
      lowerAlgo != "arc" && lowerAlgo != "clock" && lowerAlgo != "s3fifo" &&
      lowerAlgo != "sieve") {
    Napi::Error::New(env,
                     "Unsupported algorithm. Supported: lru, fifo, lfu, arc, "
                     "clock, s3fifo, sieve")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  // Open the trace file
  reader_t* reader = open_trace(tracePath.c_str(), trace_type_enum, nullptr);
  if (!reader) {
    Napi::Error::New(env, "Failed to open trace file: " + tracePath)
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  // Create a request container
  request_t* req = new_request();
  if (!req) {
    close_trace(reader);
    Napi::Error::New(env, "Failed to allocate request")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  // Initialize cache
  common_cache_params_t cc_params = {.cache_size = cacheSize,
                                     .default_ttl = 0,
                                     .hashpower = 24,
                                     .consider_obj_metadata = false};

  cache_t* cache = createCache(algorithm, cc_params);
  if (!cache) {
    close_trace(reader);
    free_request(req);
    Napi::Error::New(env, "Failed to create cache with algorithm: " + algorithm)
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  // Run simulation loop
  uint64_t n_req = 0;
  uint64_t n_miss = 0;
  uint64_t n_hit = 0;

  while (read_one_req(reader, req) == 0) {
    bool hit = cache->get(cache, req);
    if (hit)
      n_hit++;
    else
      n_miss++;
    n_req++;
  }

  // Cleanup
  close_trace(reader);
  free_request(req);
  cache->cache_free(cache);

  // Return simulation results as object
  Napi::Object result = Napi::Object::New(env);
  result.Set("totalRequests", Napi::Number::New(env, n_req));
  result.Set("hits", Napi::Number::New(env, n_hit));
  result.Set("misses", Napi::Number::New(env, n_miss));
  result.Set("hitRatio",
             Napi::Number::New(env, n_req > 0 ? (double)n_hit / n_req : 0.0));
  result.Set("missRatio",
             Napi::Number::New(env, n_req > 0 ? (double)n_miss / n_req : 0.0));
  result.Set("algorithm", Napi::String::New(env, algorithm));
  result.Set("cacheSize", Napi::Number::New(env, cacheSize));

  return result;
}

// Simple simulation with hardcoded values (backward compatibility)
Napi::Value runSim(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // Check if the default trace file exists
  if (!fileExists("../data/cloudPhysicsIO.vscsi")) {
    Napi::Error::New(
        env, "Default trace file not found: ../data/cloudPhysicsIO.vscsi")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  // === Open the trace file ===
  reader_t* reader = open_trace("../data/cloudPhysicsIO.vscsi", VSCSI_TRACE,
                                nullptr  // No special initialization parameters
  );

  if (!reader) {
    Napi::Error::New(env, "Failed to open trace").ThrowAsJavaScriptException();
    return env.Null();
  }

  // === Create a request container ===
  request_t* req = new_request();
  if (!req) {
    close_trace(reader);
    Napi::Error::New(env, "Failed to allocate request")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  // === Initialize an LRU cache ===
  common_cache_params_t cc_params = {.cache_size = 1024 * 1024,  // 1MB
                                     .default_ttl = 0,
                                     .hashpower = 24,
                                     .consider_obj_metadata = false};
  cache_t* cache = LRU_init(cc_params, nullptr);
  if (!cache) {
    close_trace(reader);
    free_request(req);
    Napi::Error::New(env, "Failed to create cache")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  // === Run simulation loop ===
  uint64_t n_req = 0;
  uint64_t n_miss = 0;
  uint64_t n_hit = 0;
  while (read_one_req(reader, req) == 0) {
    bool hit = cache->get(cache, req);
    if (hit)
      n_hit++;
    else
      n_miss++;
    n_req++;
  }

  // === Cleanup ===
  close_trace(reader);
  free_request(req);
  cache->cache_free(cache);

  // === Return results as object ===
  Napi::Object result = Napi::Object::New(env);
  result.Set("totalRequests", Napi::Number::New(env, n_req));
  result.Set("hits", Napi::Number::New(env, n_hit));
  result.Set("misses", Napi::Number::New(env, n_miss));
  result.Set("hitRatio",
             Napi::Number::New(env, n_req > 0 ? (double)n_hit / n_req : 0.0));
  result.Set("missRatio",
             Napi::Number::New(env, n_req > 0 ? (double)n_miss / n_req : 0.0));
  result.Set("algorithm", Napi::String::New(env, "lru"));
  result.Set("cacheSize", Napi::Number::New(env, 1024 * 1024));

  return result;
}

// Node.js addon initialization
Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set("runSim", Napi::Function::New(env, runSim));
  exports.Set("runSimulation", Napi::Function::New(env, runSimulation));
  return exports;
}

NODE_API_MODULE(libcachesim_addon, Init)
