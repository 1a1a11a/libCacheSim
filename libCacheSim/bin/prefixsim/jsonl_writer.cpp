#include "jsonl_writer.hpp"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <memory>

namespace prefixsim {
namespace {

void append_int(int64_t v, std::string &out) {
  char buf[24];
  const auto res = std::to_chars(buf, buf + sizeof(buf), v);
  out.append(buf, res.ptr);
}

/// An int field, or null for the -1 that marks it absent.
void append_int_or_null(int64_t v, std::string &out) {
  if (v < 0) {
    out += "null";
  } else {
    append_int(v, out);
  }
}

/// Seconds to the microsecond, as the lcp file keeps them. Printed from the
/// integer so that strtod reads back the double the lcp reader produced.
void append_seconds(double seconds, std::string &out) {
  const int64_t us = std::llround(seconds * 1e6);
  append_int(us / 1000000, out);
  char frac[7] = {'.'};
  int64_t rest = us % 1000000;
  for (int i = 6; i >= 1; --i, rest /= 10) {
    frac[i] = static_cast<char>('0' + rest % 10);
  }
  out.append(frac, sizeof(frac));
}

}  // namespace

void append_jsonl(const Request &r, std::string &out) {
  out += "{\"chat_id\":";
  append_int(r.chat_id, out);
  out += ",\"parent_chat_id\":";
  append_int(r.parent_chat_id, out);
  out += ",\"timestamp\":";
  append_seconds(r.timestamp, out);
  out += ",\"timestamp_arrival\":";
  append_seconds(r.start_timestamp, out);
  out += ",\"input_length\":";
  append_int_or_null(r.input_length, out);
  out += ",\"output_length\":";
  append_int_or_null(r.output_length, out);
  if (!r.type.empty()) {
    // as the reader took it: the text between the quotes, unescaped
    out += ",\"type\":\"";
    out += r.type;
    out += '"';
  }
  out += ",\"turn\":";
  append_int_or_null(r.turn, out);
  if (r.session != 0) {
    out += ",\"session_id\":\"";
    append_int(static_cast<int64_t>(r.session), out);
    out += '"';
  }
  if (r.user != 0) {
    out += ",\"user_id\":\"";
    append_int(static_cast<int64_t>(r.user), out);
    out += '"';
  }
  out += ",\"hash_ids\":[";
  for (size_t i = 0; i < r.blocks.size(); ++i) {
    if (i != 0) out += ',';
    append_int(static_cast<int64_t>(r.blocks[i]), out);
  }
  out += "]}";
}

bool write_jsonl(TraceReader &reader, const std::string &path,
                 int64_t &n_written, std::string &error) {
  std::unique_ptr<FILE, int (*)(FILE *)> f(fopen(path.c_str(), "w"), fclose);
  if (f == nullptr) {
    error = "cannot open '" + path + "' for writing";
    return false;
  }
  n_written = 0;
  Request request;
  std::string line;
  while (reader.next(request)) {
    line.clear();
    append_jsonl(request, line);
    line += '\n';
    if (fwrite(line.data(), 1, line.size(), f.get()) != line.size()) {
      error = "writing '" + path + "' failed";
      return false;
    }
    ++n_written;
  }
  if (fclose(f.release()) != 0) {
    error = "writing '" + path + "' failed";
    return false;
  }
  return true;
}

}  // namespace prefixsim
