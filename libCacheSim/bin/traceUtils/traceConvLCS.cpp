/* this is the converter for lcs trace format */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <numeric>
#include <unordered_map>
#include <vector>

#include "internal.hpp"
#include "libCacheSim/logging.h"
#include "libCacheSim/reader.h"
#include "traceReader/customizedReader/lcs.h"
#include "utils/include/mymath.h"

namespace traceConv {

struct obj_info {
  int64_t size;
  int32_t freq;
  int64_t last_access_vtime;
};

typedef lcs_req_v3_t lcs_req_full_t;

static void _reverse_file(std::string ofilepath, lcs_trace_stat_t stat,
                          bool output_txt, int64_t lcs_ver);
static void _write_lcs_header(std::ofstream &ofile, lcs_trace_stat_t &stat,
                              int64_t lcs_ver);
static void _analyze_trace(
    lcs_trace_stat_t &stat,
    const std::unordered_map<uint64_t, struct obj_info> &obj_map,
    const std::unordered_map<int32_t, int32_t> &tenant_cnt,
    const std::unordered_map<int32_t, int32_t> &ttl_cnt);

/**
 * @brief Convert a trace to lcs format
 *
 * @param reader
 * @param ofilepath
 * @param sample_ratio
 * @param output_txt
 * @param remove_size_change
 * @param lcs_ver       the version of lcs format, see lcs.h for more details
 */
void convert_to_lcs(reader_t *reader, std::string ofilepath, bool output_txt,
                    bool remove_size_change, int lcs_ver) {
  request_t *req = new_request();
  std::ofstream ofile_temp(ofilepath + ".reverse",
                           std::ios::out | std::ios::binary | std::ios::trunc);
  std::unordered_map<uint64_t, struct obj_info> obj_map;
  std::unordered_map<int32_t, int32_t> tenant_cnt;
  std::unordered_map<int32_t, int32_t> ttl_cnt;
  int n_features = LCS_VER_TO_N_FEATURES[lcs_ver];

  lcs_trace_stat_t stat;
  memset(&stat, 0, sizeof(stat));
  stat.version = CURR_STAT_VERSION;
  int64_t n_req_total = get_num_of_req(reader);
  obj_map.reserve(n_req_total / 100 + 1e4);

  INFO("%s: %.2f M requests in total\n", reader->trace_path,
       (double)n_req_total / 1.0e6);

  reader->read_direction = READ_BACKWARD;
  reader_set_read_pos(reader, 1.0);
  go_back_one_req(reader);
  read_one_req(reader, req);

  // because we read backwards, the first request is the last request in the
  // trace
  stat.end_timestamp = req->clock_time;

  while (true) {
    if (lcs_ver == 1 || lcs_ver == 2) {
      if (req->clock_time > UINT32_MAX) {
        WARN(
            "clock_time %lld > UINT32_MAX, may cause overflow consider using "
            "lcs_ver 3\n",
            (long long)req->clock_time);
      }
      if (req->obj_size > UINT32_MAX) {
        WARN(
            "obj_size %lld > UINT32_MAX, may cause overflow consider using "
            "lcs_ver 3\n",
            (long long)req->obj_size);
      }
    }

    auto info_it = obj_map.find(req->obj_id);

    if (info_it == obj_map.end()) {
      req->next_access_vtime = INT64_MAX;
      stat.n_obj++;
      stat.n_obj_byte += req->obj_size;
      struct obj_info info = {req->obj_size, 1, stat.n_req};
      obj_map[req->obj_id] = info;
    } else {
      req->next_access_vtime = info_it->second.last_access_vtime;
      info_it->second.last_access_vtime = stat.n_req;
      info_it->second.freq++;
      if (info_it->second.size != req->obj_size) {
        if (!remove_size_change) {
          WARN(
              "find object size change, prev %lld new %lld, please enable "
              "remove_size_change\n",
              (long long)info_it->second.size, (long long)req->obj_size);
        } else {
          req->obj_size = info_it->second.size;
        }
      }
    }

    lcs_req_full_t lcs_req;
    lcs_req.clock_time = req->clock_time;
    lcs_req.obj_id = req->obj_id;
    lcs_req.obj_size = req->obj_size;
    lcs_req.op = req->op;
    lcs_req.tenant = req->tenant_id;
    lcs_req.ttl = req->ttl;
    lcs_req.next_access_vtime = req->next_access_vtime;

    if (lcs_req.op == OP_GET || lcs_req.op == OP_GETS ||
        lcs_req.op == OP_READ) {
      stat.n_read++;
    } else if (lcs_req.op == OP_WRITE || lcs_req.op == OP_SET ||
               lcs_req.op == OP_REPLACE || lcs_req.op == OP_ADD ||
               lcs_req.op == OP_UPDATE) {
      stat.n_write++;
    } else if (lcs_req.op == OP_DELETE) {
      stat.n_delete++;
    }

    if (tenant_cnt.find(lcs_req.tenant) == tenant_cnt.end()) {
      tenant_cnt[lcs_req.tenant] = 1;
      stat.n_tenant++;
    } else {
      tenant_cnt[lcs_req.tenant]++;
    }

    if (ttl_cnt.find(req->ttl) == ttl_cnt.end()) {
      ttl_cnt[req->ttl] = 1;
      stat.n_ttl++;
    } else {
      ttl_cnt[req->ttl]++;
    }

    ofile_temp.write(reinterpret_cast<char *>(&lcs_req),
                     sizeof(lcs_req_full_t));
    for (int i = 0; i < n_features; i++) {
      ofile_temp.write(reinterpret_cast<char *>(&req->features[i]),
                       sizeof(int32_t));
    }

    stat.n_req_byte += req->obj_size;
    stat.n_req += 1;

    if (stat.n_req % 100000000 == 0) {
      INFO(
          "%s: %ld M requests (%.2lf GB), trace time %ld, working set %lld "
          "object, %lld B (%.2lf GB)\n",
          reader->trace_path, (long)(stat.n_req / 1e6),
          (double)stat.n_req_byte / GiB,
          (long)(stat.end_timestamp - req->clock_time), (long long)stat.n_obj,
          (long long)stat.n_obj_byte, (double)stat.n_obj_byte / GiB);
    }

    if (stat.n_req > n_req_total * 2) {
      ERROR("n_req_curr (%lld) > n_req_total (%lld)\n", (long long)stat.n_req,
            (long long)n_req_total);
    }

    if (read_one_req_above(reader, req) != 0) {
      break;
    }
  }

  stat.start_timestamp = req->clock_time;

  if (reader->sampler == nullptr) {
    assert(stat.n_req == get_num_of_req(reader));
  }

  free_request(req);
  ofile_temp.close();

  _analyze_trace(stat, obj_map, tenant_cnt, ttl_cnt);

  _reverse_file(ofilepath, stat, output_txt, lcs_ver);
}

static void _write_lcs_header(std::ofstream &ofile, lcs_trace_stat_t &stat,
                              int64_t lcs_ver) {
  lcs_trace_header_t lcs_header;
  memset(&lcs_header, 0, sizeof(lcs_header));
  lcs_header.start_magic = LCS_TRACE_START_MAGIC;
  lcs_header.end_magic = LCS_TRACE_END_MAGIC;
  lcs_header.stat = stat;
  lcs_header.version = lcs_ver;

  ofile.write(reinterpret_cast<char *>(&lcs_header),
              sizeof(lcs_trace_header_t));
}

static void _analyze_trace(
    lcs_trace_stat_t &stat,
    const std::unordered_map<uint64_t, struct obj_info> &obj_map,
    const std::unordered_map<int32_t, int32_t> &tenant_cnt,
    const std::unordered_map<int32_t, int32_t> &ttl_cnt) {
  INFO("########################################\n");
  INFO(
      "trace stat: n_req %lld, n_obj %lld, n_byte %lld (%.2lf GiB), "
      "n_uniq_byte %lld (%.2lf GiB)\n",
      (long long)stat.n_req, (long long)stat.n_obj, (long long)stat.n_req_byte,
      (double)stat.n_req_byte / GiB, (long long)stat.n_obj_byte,
      (double)stat.n_obj_byte / GiB);
  INFO("n_read %lld, n_write %lld, n_delete %lld\n", (long long)stat.n_read,
       (long long)stat.n_write, (long long)stat.n_delete);

  INFO("start time %lld, end time %lld, duration %lld seconds %.2lf days\n",
       (long long)stat.start_timestamp, (long long)stat.end_timestamp,
       (long long)(stat.end_timestamp - stat.start_timestamp),
       (double)(stat.end_timestamp - stat.start_timestamp) / (24 * 3600.0));

  /**** analyze object size ****/
  std::unordered_map<int64_t, int32_t> size_cnt;
  stat.smallest_obj_size = INT64_MAX;
  stat.largest_obj_size = 0;
  for (const auto &kv : obj_map) {
    if (size_cnt.find(kv.second.size) == size_cnt.end()) {
      size_cnt[kv.second.size] = 1;
    } else {
      size_cnt[kv.second.size]++;
    }
    if (kv.second.size < stat.smallest_obj_size) {
      stat.smallest_obj_size = kv.second.size;
    }
    if (kv.second.size > stat.largest_obj_size) {
      stat.largest_obj_size = kv.second.size;
    }
  }

  std::vector<std::pair<int64_t, int32_t>> size_cnt_vec(size_cnt.begin(),
                                                        size_cnt.end());
  std::sort(size_cnt_vec.begin(), size_cnt_vec.end(),
            [](const auto &a, const auto &b) { return a.second > b.second; });
  for (size_t i = 0; i < std::min(size_cnt_vec.size(), (size_t)N_MOST_COMMON);
       i++) {
    stat.most_common_obj_sizes[i] = size_cnt_vec[i].first;
    stat.most_common_obj_size_ratio[i] =
        (float)size_cnt_vec[i].second / stat.n_obj;
  }

  INFO("object size: smallest %lld, largest %lld\n",
       (long long)stat.smallest_obj_size, (long long)stat.largest_obj_size);
  INFO(
      "most common object sizes (req fraction): %lld(%.4lf) %lld(%.4lf) "
      "%lld(%.4lf) %lld(%.4lf)...\n",
      (long long)stat.most_common_obj_sizes[0],
      stat.most_common_obj_size_ratio[0],
      (long long)stat.most_common_obj_sizes[1],
      stat.most_common_obj_size_ratio[1],
      (long long)stat.most_common_obj_sizes[2],
      stat.most_common_obj_size_ratio[2],
      (long long)stat.most_common_obj_sizes[3],
      stat.most_common_obj_size_ratio[3]);

  /**** analyze object popularity ****/
  std::unordered_map<int32_t, int32_t> freq_cnt;
  for (const auto &kv : obj_map) {
    if (freq_cnt.find(kv.second.freq) == freq_cnt.end()) {
      freq_cnt[kv.second.freq] = 1;
    } else {
      freq_cnt[kv.second.freq]++;
    }
  }

  // sort by freq
  std::vector<std::pair<int32_t, int32_t>> freq_cnt_vec(freq_cnt.begin(),
                                                        freq_cnt.end());
  std::sort(freq_cnt_vec.begin(), freq_cnt_vec.end(),
            [](const auto &a, const auto &b) { return a.first > b.first; });
  for (size_t i = 0; i < std::min(freq_cnt_vec.size(), (size_t)N_MOST_COMMON);
       i++) {
    stat.highest_freq[i] = freq_cnt_vec[i].first;
  }

  /* calculate Zipf alpha using linear regression */
  double *log_freq = new double[stat.n_obj];
  double *log_rank = new double[stat.n_obj];
  int64_t n = 0;
  for (size_t i = 0; i < freq_cnt_vec.size(); i++) {
    for (int j = 0; j < freq_cnt_vec[i].second; j++) {
      log_freq[n] = log(static_cast<double>(freq_cnt_vec[i].first));
      log_rank[n] = log(static_cast<double>(n + 1));
      n++;
    }
  }
  assert(n == stat.n_obj);

  double slope, intercept;
  linear_regression(log_rank, log_freq, n, &slope, &intercept);

  stat.skewness = -slope;

  // sort by freq count
  std::sort(freq_cnt_vec.begin(), freq_cnt_vec.end(),
            [](const auto &a, const auto &b) { return a.second > b.second; });
  for (size_t i = 0; i < std::min(freq_cnt_vec.size(), (size_t)N_MOST_COMMON);
       i++) {
    stat.most_common_freq[i] = freq_cnt_vec[i].first;
    stat.most_common_freq_ratio[i] = (float)freq_cnt_vec[i].second / stat.n_obj;
  }

  INFO("highest freq: %lld %lld %lld %lld skewness %.4lf\n",
       (long long)stat.highest_freq[0], (long long)stat.highest_freq[1],
       (long long)stat.highest_freq[2], (long long)stat.highest_freq[3],
       stat.skewness);
  INFO(
      "most common freq (req fraction): %d(%.4lf) %d(%.4lf) %d(%.4lf) "
      "%d(%.4lf)...\n",
      stat.most_common_freq[0], stat.most_common_freq_ratio[0],
      stat.most_common_freq[1], stat.most_common_freq_ratio[1],
      stat.most_common_freq[2], stat.most_common_freq_ratio[2],
      stat.most_common_freq[3], stat.most_common_freq_ratio[3]);

  /**** analyze tenant ****/
  stat.n_tenant = tenant_cnt.size();
  std::vector<std::pair<int32_t, int32_t>> tenant_cnt_vec(tenant_cnt.begin(),
                                                          tenant_cnt.end());
  std::sort(tenant_cnt_vec.begin(), tenant_cnt_vec.end(),
            [](const auto &a, const auto &b) { return a.second > b.second; });
  for (size_t i = 0; i < std::min(tenant_cnt_vec.size(), (size_t)N_MOST_COMMON);
       i++) {
    stat.most_common_tenants[i] = tenant_cnt_vec[i].first;
    stat.most_common_tenant_ratio[i] =
        (float)tenant_cnt_vec[i].second / stat.n_req;
  }
  if (stat.n_tenant > 1) {
    INFO("#tenant: %ld\n", (long)stat.n_tenant);
    INFO(
        "most common tenants (req fraction): %d(%.4lf) %d(%.4lf) %d(%.4lf) "
        "%d(%.4lf)...\n",
        stat.most_common_tenants[0], stat.most_common_tenant_ratio[0],
        stat.most_common_tenants[1], stat.most_common_tenant_ratio[1],
        stat.most_common_tenants[2], stat.most_common_tenant_ratio[2],
        stat.most_common_tenants[3], stat.most_common_tenant_ratio[3]);
  }

  /**** analyze ttl ****/
  stat.n_ttl = ttl_cnt.size();
  std::vector<std::pair<int32_t, int32_t>> ttl_cnt_vec(ttl_cnt.begin(),
                                                       ttl_cnt.end());
  std::sort(ttl_cnt_vec.begin(), ttl_cnt_vec.end(),
            [](const auto &a, const auto &b) { return a.second > b.second; });
  stat.smallest_ttl = std::min_element(ttl_cnt_vec.begin(), ttl_cnt_vec.end(),
                                       [](const auto &a, const auto &b) {
                                         return a.first < b.first;
                                       })
                          ->first;
  stat.largest_ttl = std::max_element(ttl_cnt_vec.begin(), ttl_cnt_vec.end(),
                                      [](const auto &a, const auto &b) {
                                        return a.first < b.first;
                                      })
                         ->first;
  for (size_t i = 0; i < std::min(ttl_cnt_vec.size(), (size_t)N_MOST_COMMON);
       i++) {
    stat.most_common_ttls[i] = ttl_cnt_vec[i].first;
    stat.most_common_ttl_ratio[i] = (float)ttl_cnt_vec[i].second / stat.n_req;
  }
  if (stat.n_ttl > 1) {
    INFO("#ttl: %ld\n", (long)stat.n_ttl);
    INFO("smallest ttl: %ld, largest ttl: %ld\n", (long)stat.smallest_ttl,
         (long)stat.largest_ttl);
    INFO(
        "most common ttls (req fraction): %d(%.4lf) %d(%.4lf) %d(%.4lf) "
        "%d(%.4lf)...\n",
        stat.most_common_ttls[0], stat.most_common_ttl_ratio[0],
        stat.most_common_ttls[1], stat.most_common_ttl_ratio[1],
        stat.most_common_ttls[2], stat.most_common_ttl_ratio[2],
        stat.most_common_ttls[3], stat.most_common_ttl_ratio[3]);
  }
  INFO("########################################\n");
}

/**
 * @brief read the reverse trace and write to the output file
 *
 * @param ofilepath
 * @param stat
 * @param output_txt
 * @param lcs_ver
 */
static void _reverse_file(std::string ofilepath, lcs_trace_stat_t stat,
                          bool output_txt, int64_t lcs_ver) {
  size_t file_size;
  char *mapped_file = reinterpret_cast<char *>(
      utils::setup_mmap(ofilepath + ".reverse", &file_size));
  size_t pos = file_size;

  std::ofstream ofile(ofilepath,
                      std::ios::out | std::ios::binary | std::ios::trunc);
  _write_lcs_header(ofile, stat, lcs_ver);

  INFO("start to reverse the trace...\n");
  std::ofstream ofile_txt;
  if (output_txt)
    ofile_txt.open(ofilepath + ".txt", std::ios::out | std::ios::trunc);

  lcs_req_full_t lcs_req_full;
  size_t lcs_full_req_entry_size = sizeof(lcs_req_full_t);

  // for lcs version 4-8, we need to read the features
  size_t n_features = 0;
  if (lcs_ver >= 4 && lcs_ver <= 8) {
    n_features = LCS_VER_TO_N_FEATURES[lcs_ver];
  }

  size_t entry_size = lcs_full_req_entry_size + n_features * sizeof(int32_t);

  while (pos >= entry_size) {
    pos -= entry_size;
    memcpy(&lcs_req_full, mapped_file + pos, lcs_full_req_entry_size);
    if (lcs_req_full.next_access_vtime != INT64_MAX) {
      /* req.next_access_vtime is the vtime start from the end */
      lcs_req_full.next_access_vtime =
          stat.n_req - lcs_req_full.next_access_vtime;
    }

    if (lcs_ver == 1) {
      lcs_req_v1_t lcs_req_v1;
      lcs_req_v1.clock_time = lcs_req_full.clock_time;
      lcs_req_v1.obj_id = lcs_req_full.obj_id;
      lcs_req_v1.obj_size = lcs_req_full.obj_size;
      lcs_req_v1.next_access_vtime = lcs_req_full.next_access_vtime;

      ofile.write(reinterpret_cast<char *>(&lcs_req_v1), sizeof(lcs_req_v1));
    } else if (lcs_ver == 2) {
      lcs_req_v2_t lcs_req_v2;
      lcs_req_v2.clock_time = lcs_req_full.clock_time;
      lcs_req_v2.obj_id = lcs_req_full.obj_id;
      lcs_req_v2.obj_size = lcs_req_full.obj_size;
      lcs_req_v2.op = lcs_req_full.op;
      lcs_req_v2.tenant = lcs_req_full.tenant;
      lcs_req_v2.next_access_vtime = lcs_req_full.next_access_vtime;

      ofile.write(reinterpret_cast<char *>(&lcs_req_v2), sizeof(lcs_req_v2));
    } else if (lcs_ver == 3) {
      lcs_req_v3_t lcs_req_v3;
      lcs_req_v3.clock_time = lcs_req_full.clock_time;
      lcs_req_v3.obj_id = lcs_req_full.obj_id;
      lcs_req_v3.ttl = lcs_req_full.ttl;
      lcs_req_v3.obj_size = lcs_req_full.obj_size;
      lcs_req_v3.op = lcs_req_full.op;
      lcs_req_v3.tenant = lcs_req_full.tenant;
      lcs_req_v3.next_access_vtime = lcs_req_full.next_access_vtime;

      ofile.write(reinterpret_cast<char *>(&lcs_req_v3), sizeof(lcs_req_v3));
    } else if (lcs_ver >= 4 && lcs_ver <= 8) {
      lcs_req_v3_t base;
      base.clock_time = lcs_req_full.clock_time;
      base.obj_id = lcs_req_full.obj_id;
      base.ttl = lcs_req_full.ttl;
      base.obj_size = lcs_req_full.obj_size;
      base.op = lcs_req_full.op;
      base.tenant = lcs_req_full.tenant;
      base.next_access_vtime = lcs_req_full.next_access_vtime;

      ofile.write(reinterpret_cast<char *>(&base), sizeof(lcs_req_v3));
      ofile.write(mapped_file + pos + lcs_full_req_entry_size,
                  n_features * sizeof(int32_t));
    } else {
      ERROR("invalid lcs version %lld\n", (long long)lcs_ver);
    }

    if (output_txt) {
      ofile_txt << lcs_req_full.clock_time << "," << lcs_req_full.obj_id << ","
                << lcs_req_full.obj_size << ","
                << lcs_req_full.next_access_vtime << "\n";
    }
  }

  munmap(mapped_file, file_size);
  ofile.close();
  if (output_txt) ofile_txt.close();

  remove((ofilepath + ".reverse").c_str());

  INFO("trace conversion finished, output %s\n", ofilepath.c_str());
}

}  // namespace traceConv
