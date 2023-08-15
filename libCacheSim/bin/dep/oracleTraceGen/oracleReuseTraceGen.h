#pragma once

#include "../traceReader/reader.h"
#include "enum.h"
#include "utilsPrint.h"
#include "../utils/include/utils.h"
#include "../lib/robin_hood.h"

#include <boost/dynamic_bitset.hpp>

#include <iostream>
#include <fstream>
#include <vector>
#include <cstdio>
#include <vector>
#include <string>
#include <limits>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <memory>
#include <unordered_map>


namespace oracleTraceGen {

/* this considers update and delete, which resets next_access_vtime_ */
struct reqEntryReuseRealSystemTwrNS {
  // IQHIHHIQ
  uint32_t real_time_;
  uint64_t obj_id_;
  uint16_t key_size_;
  uint32_t val_size_;
  uint16_t op_;
  uint16_t ns;    /* namespace */
  int32_t ttl_;
  int64_t next_access_vtime_;

  int get_obj_size() {
    return key_size_ + val_size_;
  }
  void set_obj_size(int obj_size) {
    ;
  }
  bool is_size_zero() {
    return val_size_ == 0 && (op_ == OP_GET || op_ == OP_GETS);
  }
  bool is_op_write_or_delete() {
    /* add is not counted as write, see enum.h for op definition */
    return op_ == 3 || (op_ >= 5 && op_ <= 9);
  }
  int32_t get_ttl() {
    return ttl_;
  }
  void set_ttl(int32_t ttl) {
    ttl_ = ttl;
  }
  uint32_t get_real_time() {
    return real_time_;
  }
  void init(request_t *req) {
    this->real_time_ = req->real_time;
    this->obj_id_ = req->obj_id;
    this->key_size_ = req->key_size;
    this->val_size_ = req->val_size;
    this->ns = req->obj_namespace;
    this->op_ = req->op;
    this->ttl_ = req->ttl;
  }
}__attribute__((packed));

// ts, obj, sz, ttl, age, hostname, content (h), extension (h), n_level, n_param, method, colo
struct reqEntryReuseCF1 {
  uint32_t real_time_;
  uint64_t obj_id_;
  uint64_t obj_size_;
  uint32_t ttl_;
  uint32_t age_;
  uint32_t hostname_;
  int64_t next_access_vtime_;
  uint16_t content_type_;
  uint16_t extension_;
  uint16_t n_level_;
  uint8_t n_param_;
  uint8_t method_;
  uint8_t colo_;

  int get_obj_size() {
    return obj_size_;
  }
  void set_obj_size(int obj_size) {
    this->obj_size_ = obj_size;
  }
  bool is_size_zero() {
    return obj_size_ == 0;
  }
  bool is_op_write_or_delete() {
    return false;
  }
  int32_t get_ttl() {
    return ttl_;
  }
  void set_ttl(int32_t ttl) {
    ttl_ = ttl;
  }
  uint32_t get_real_time() {
    return real_time_;
  }
  void init(request_t *req) {
    this->real_time_ = req->real_time;
    this->obj_id_ = req->obj_id;
    this->obj_size_ = req->obj_size;
    this->ttl_ = req->ttl;
    this->age_ = req->age;
    this->hostname_ = req->hostname;
    this->extension_ = req->extension;
    this->n_level_ = req->n_level;
    this->n_param_ = req->n_param;
    this->method_ = req->method;
    this->colo_ = req->colo;

    this->content_type_ = req->content_type;
  }
}__attribute__((packed));

struct reqEntryReuseAkamai {
  uint32_t real_time_;
  uint64_t obj_id_;
  uint32_t obj_size_;
  int16_t customer_id_;
  int16_t bucket_id_;
  int16_t content_type_;
  int64_t next_access_vtime_;

  int get_obj_size() {
    return obj_size_;
  }
  void set_obj_size(int obj_size) {
    this->obj_size_ = obj_size;
  }
  bool is_size_zero() {
    return obj_size_ == 0;
  }
  bool is_op_write_or_delete() {
    return false;
  }
  int32_t get_ttl() {
    return 0;
  }
  void set_ttl(int32_t ttl) {
    ;
  }
  uint32_t get_real_time() {
    return real_time_;
  }
  void init(request_t *req) {
    this->real_time_ = req->real_time;
    this->obj_id_ = req->obj_id;
    this->obj_size_ = req->obj_size;
    this->customer_id_ = req->tenant_id;
    this->bucket_id_ = req->bucket_id;
    this->content_type_ = req->content_type;
  }
}__attribute__((packed));

struct reqEntryReuseWiki16u {
  uint64_t obj_id_;
  uint32_t obj_size_;
  int16_t content_type_;
  int64_t next_access_vtime_;

  int get_obj_size() {
    return obj_size_;
  }
  void set_obj_size(int obj_size) {
    this->obj_size_ = obj_size;
  }
  bool is_op_write_or_delete() {
    return false;
  }
  bool is_size_zero() {
    return obj_size_ == 0;
  }
  int32_t get_ttl() {
    return 0;
  }
  void set_ttl(int32_t ttl_) {
    ;
  }
  uint32_t get_real_time() {
    return 0;
  }
  void init(request_t *req) {
    this->obj_id_ = req->obj_id;
    this->obj_size_ = req->obj_size;
    this->content_type_ = req->content_type;
  }
}__attribute__((packed));

struct reqEntryReuseWiki19u {
  uint32_t real_time_;
  uint64_t obj_id_;
  uint32_t obj_size_;
  int16_t content_type_;
  int64_t next_access_vtime_;

  int get_obj_size() {
    return obj_size_;
  }
  void set_obj_size(int obj_size) {
    this->obj_size_ = obj_size;
  }
  bool is_size_zero() {
    return obj_size_ == 0;
  }
  bool is_op_write_or_delete() {
    return false;
  }
  int32_t get_ttl() {
    return 0;
  }
  void set_ttl(int32_t ttl_) {
    ;
  }
  uint32_t get_real_time() {
    return real_time_;
  }
  void init(request_t *req) {
    this->real_time_ = req->real_time;
    this->obj_id_ = req->obj_id;
    this->obj_size_ = req->obj_size;
    this->content_type_ = req->content_type;
  }
}__attribute__((packed));

struct reqEntryReuseGeneral {
  uint32_t real_time_;
  uint64_t obj_id_;
  uint32_t obj_size_;
  int64_t next_access_vtime_;

  int get_obj_size() {
    return obj_size_;
  }
  void set_obj_size(int obj_size) {
    this->obj_size_ = obj_size;
  }
  bool is_size_zero() {
    return obj_size_ == 0;
  }
  bool is_op_write_or_delete() {
    return false;
  }
  int32_t get_ttl() {
    return 0;
  }
  void set_ttl(int32_t ttl) {
    ;
  }
  uint32_t get_real_time() {
    return real_time_;
  }
  void init(request_t *req) {
    this->real_time_ = req->real_time;
    this->obj_id_ = req->obj_id;
    this->obj_size_ = req->obj_size;
    if (req->obj_size >= UINT32_MAX) {
      printf("obj_size %ld is too large, set to UINT32_MAX\n", req->obj_size);
      this->obj_size_ = UINT32_MAX;
    }
  }
}__attribute__((packed));

/* this considers update and delete, which resets next_access_vtime_ */
struct reqEntryReuseGeneralOpNS {
  // IqIbhq
  uint32_t real_time_;
  uint64_t obj_id_;
  uint32_t obj_size_;
  uint8_t op_;
  uint16_t ns;    /* namespace */
  int64_t next_access_vtime_;

  int get_obj_size() {
    return obj_size_;
  }
  void set_obj_size(int obj_size) {
    this->obj_size_ = obj_size;
  }
  bool is_size_zero() {
    return obj_size_ == 0;
  }
  bool is_op_write_or_delete() {
    /* add is not counted as write, see enum.h for op definition */
    return op_ == 3;
  }
  int32_t get_ttl() {
    return 0;
  }
  void set_ttl(int32_t ttl) {
    ;
  }
  uint32_t get_real_time() {
    return real_time_;
  }
  void init(request_t *req) {
    this->real_time_ = req->real_time;
    this->obj_id_ = req->obj_id;
    this->obj_size_ = req->obj_size;
    this->ns = req->obj_namespace;
    this->op_ = req->op;
  }
}__attribute__((packed));


template<typename T>
class OracleReuseTraceGen {
 public:
  OracleReuseTraceGen(reader_t *reader, std::string ofile_path) :
      reader_(reader),
      ofile_path_(std::move(ofile_path)) {
    temp_ofile_path_ = ofile_path_ + ".reverse";
    ofile_temp_.open(temp_ofile_path_, std::ios::out | std::ios::binary | std::ios::trunc);
    ofile_.open(ofile_path_, std::ios::out |std:: ios::binary | std::ios::trunc);
    if (!ofile_) {
      std::cerr << "cannot open output file " << ofile_path_ << std::endl;
    }
  }

  ~OracleReuseTraceGen() {
    if (std::remove(temp_ofile_path_.c_str()) != 0) {
      perror("unable to remove the temporary file\n");
    }
  };
  OracleReuseTraceGen(const OracleReuseTraceGen &seg_orcale) = delete;
  OracleReuseTraceGen(const OracleReuseTraceGen &&seg_orcale) = delete;
  OracleReuseTraceGen &operator=(const OracleReuseTraceGen &o) = delete;
  OracleReuseTraceGen &operator=(const OracleReuseTraceGen &&o) = delete;


  struct last_access_info {
    int64_t vtime;
    int32_t ttl;
  }__attribute__((packed));
  void run(bool output_txt, bool backfill_ttl, bool remove_size_change) {
    request_t *req = new_request();
    auto *last_access= new robin_hood::unordered_flat_map<uint64_t, struct last_access_info>;

    /* n_req_written counts the number of requests written to file,
     * n_req_total is the total number of requests in the trace
     * n_req_curr is the number of requests scanned, this is different from n_req_written when
     * we remove miss caused fills in twr traces */
    uint64_t n_req_written = 0, n_req_curr = 0, n_req_total = get_num_of_req(reader_);
#ifndef __APPLE__
    last_access->reserve(1e8);
#endif

    reader_->ignore_size_zero_req = FALSE;
    /* we remove cache miss caused on-demand fill for twr trace */
    boost::dynamic_bitset<> *is_cache_miss_bitset = nullptr;
    int64_t n_miss_fill = 0;
    if (typeid(T) == typeid(struct reqEntryReuseRealSystemTwrNS)) {
      is_cache_miss_bitset = std::move(gen_miss_fill_vec(&n_miss_fill));
    }

    int status;
    uint64_t unique_bytes = 0, total_bytes = 0;
    int64_t n_ttl_missing = 0, n_size_zero = 0;
    utilsPrint::print_time();
    std::cout << reader_->trace_path << ": " << (double) n_req_total / 1000000.0 << "M requests, " << n_miss_fill << " miss fill" << std::endl;

    /* we read reversely and ignore_size_zero_req will read forward */
    reader_set_read_pos(reader_, 1.0);
    go_back_one_line(reader_);
    read_one_req(reader_, req);

    int64_t start_ts = req->real_time;

    T *re = new T;
    re->init(req);
    int32_t last_ttl;
    int64_t last_access_vtime;

    while (true) {
      if (typeid(T) == typeid(struct reqEntryReuseRealSystemTwrNS) && (*is_cache_miss_bitset)[n_req_total - n_req_curr - 1]) {
        /* this is on-demand fill, because we remove the get miss (they have zero size),
         * we change the on-demand fill to GET request, which will cause an on-demand fill if it is cache miss */
        req->op = OP_GET;
      }

      last_ttl = 0;
      auto last_access_it = last_access->find(re->obj_id_);

      if (backfill_ttl) {
        if (re->get_ttl() != 0) {
          last_ttl = re->get_ttl();
        } else {
          if (last_access_it != last_access->end()) {
            re->set_ttl(last_access_it->second.ttl);
            last_ttl = last_access_it->second.ttl;
          } else {
            n_ttl_missing += 1;
          }
        }
      }

      if (last_access_it == last_access->end()) {
        re->next_access_vtime_ = -1;
        unique_bytes += req->obj_size;
      } else {
        re->next_access_vtime_ = last_access_it->second.vtime;
      }

      total_bytes += req->obj_size;

      last_access_vtime = (int64_t) n_req_written;

      if (typeid(T) == typeid(struct reqEntryReuseRealSystemTwrNS) && re->is_op_write_or_delete()) {
        /* if this request is write or delete */
        last_access_vtime = -1;
      }


      (*last_access)[req->obj_id] = {.vtime = last_access_vtime, .ttl = last_ttl};

      ofile_temp_.write(reinterpret_cast<char *>(re), sizeof(T));
      n_req_written += 1;

      if (n_req_curr % 100000000 == 0 && n_req_curr != 0) {
        utilsPrint::print_time();
        std::cout << std::fixed << std::setprecision(2) << (double) n_req_curr / 1000000 << " MReq, ts " << -req->real_time + start_ts
                  << ", working set/req GB " << (double) unique_bytes / GiB << "/" << (double) total_bytes / GiB << std::endl;
      }

      status = read_one_req_above(reader_, req);
      re->init(req);
      n_req_curr += 1;

      while (re->is_size_zero() && status == 0) {
        /* we skip zero-sized request, which is either something does not exist
         * or a cache miss, and the latter will trigger an on-demand fill */
        n_size_zero += 1;
        status = read_one_req_above(reader_, req);
        re->init(req);
        n_req_curr += 1;
      }

      if (status != 0)
        break;
    }

    utils::my_assert(n_req_curr == n_req_total, "n_req_curr != n_req_total");

    delete re;
    free_request(req);

    ofile_temp_.close();
    delete last_access;
    delete is_cache_miss_bitset;

    utilsPrint::print_time();
    std::cout << ofile_path_  << " ";
    std::cout << n_size_zero << "(" << (double) n_size_zero/(double) n_req_total * 100 << "%) size zero requests";
    if (backfill_ttl) {
      std::cout << ", missing ttl " << n_ttl_missing << "/" << n_req_written << " (" << (double) n_ttl_missing / (double) n_req_written << ")\n";
    } else {
      std::cout << std::endl;
    }

    _reverse_file(n_req_written, output_txt, remove_size_change);
  }

 private:
  reader_t *reader_;
  std::string ofile_path_;
  std::string temp_ofile_path_;
  std::ofstream ofile_;
  std::ofstream ofile_txt_;
  /* TODO: change to tempfile */
  std::ofstream ofile_temp_;

  boost::dynamic_bitset<> *gen_miss_fill_vec(int64_t *n_fill) {
    /* for twr traces, find whether a cache write is an on-demand fill */
    uint64_t n_req = get_num_of_req(reader_);
    boost::dynamic_bitset<> *is_cache_miss_bitset = new boost::dynamic_bitset<>(n_req);
    robin_hood::unordered_flat_map<obj_id_t, int64_t> last_miss_ts;

    request_t *req = new_request();

    read_one_req(reader_, req);
    int64_t n_req_curr = 1;
    while (req->valid) {
//      printf("%ld time %ld obj_id %lu obj_size %d + %u op %d namespace %d ttl %d next %ld xxxxxxxx \n", (long) n_req_curr,
//             (long) req->real_time, (unsigned long) req->obj_id, (int) req->key_size, (unsigned) req->val_size, (int) req->op,
//             (int) req->obj_namespace, (int) req->ttl, (long) req->next_access_vtime);

      if (req->val_size == 0 && (req->op == OP_GET || req->op == OP_GETS)) {
        last_miss_ts[req->obj_id] = req->real_time;
      }

      if (req->op == OP_SET || req->op == OP_ADD || req->op == OP_CAS || req->op == OP_REPLACE) {
        auto it = last_miss_ts.find(req->obj_id);
        if (it != last_miss_ts.end() && req->real_time - it->second <= 1) {
          (*is_cache_miss_bitset)[n_req_curr] = 1;
          *n_fill += 1;
        }
      }

      read_one_req(reader_, req);
      n_req_curr += 1;
    }
    utils::my_assert(n_req_curr -1 == n_req, "n_req_curr - 1 != n_req");

    free_request(req);
    reset_reader(reader_);

    return is_cache_miss_bitset;
  }

  void *_setup_mmap(std::string &file_path, size_t *size) {
    int fd;
    struct stat st{};
    void *mapped_file;

    // set up mmap region
    if ((fd = open(file_path.c_str(), O_RDONLY)) < 0) {
      ERROR("Unable to open '%s', %s\n", file_path.c_str(), strerror(errno));
      exit(1);
    }

    if ((fstat(fd, &st)) < 0) {
      close(fd);
      ERROR("Unable to fstat '%s', %s\n", file_path.c_str(), strerror(errno));
      exit(1);
    }

    *size = st.st_size;
    mapped_file = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if ((mapped_file) == MAP_FAILED) {
      close(fd);
      mapped_file = nullptr;
      ERROR("Unable to allocate %llu bytes of memory, %s\n",
            (unsigned long long) st.st_size, strerror(errno));
      abort();
    }

#ifdef MADV_HUGEPAGE
    int mstatus = madvise(mapped_file, st.st_size, MADV_HUGEPAGE | MADV_SEQUENTIAL);
    if (mstatus != 0) {
      ERROR("cannot turn on hugepage %s\n", strerror(errno));
    }
#endif


    close(fd);
    return mapped_file;
  }

  void _reverse_file(uint64_t n_total_req, bool output_txt, bool remove_size_change) {
    utilsPrint::print_time();
    uint64_t n_req = 0;
    size_t file_size;
    char *mapped_file = reinterpret_cast<char *>(_setup_mmap(temp_ofile_path_, &file_size));
    size_t pos = file_size;

    if (output_txt)
      ofile_txt_.open(ofile_path_ + ".txt", std::ios::out | std::ios::trunc);

    /* we remove object size change because some of the systems do not allow object size change */
//    std::unordered_map<uint64_t, uint32_t> last_obj_size;
    robin_hood::unordered_flat_map<uint64_t, uint32_t> last_obj_size;

#ifndef __APPLE__
    last_obj_size.reserve(1e8);
#endif

    T re;
    size_t req_entry_size = sizeof(re);
    std::cout << "reverse output " << n_total_req << " requests, " << "entry size " << req_entry_size << std::endl;

    while (pos >= req_entry_size) {
      pos -= req_entry_size;
      memcpy(&re, mapped_file + pos, req_entry_size);
      if (re.next_access_vtime_ != -1) {
        /* re.next_access_vtime_ is the vtime start from the end */
        re.next_access_vtime_ = n_total_req - 1 - re.next_access_vtime_;
      }

      int32_t obj_size = re.get_obj_size();
      if (remove_size_change) {
        auto it = last_obj_size.find(re.obj_id_);
        if (it != last_obj_size.end()) {
          obj_size = it->second;
          re.set_obj_size(obj_size);
        } else {
          last_obj_size[re.obj_id_] = obj_size;
        }
      }


      ofile_.write(reinterpret_cast<char *>(&re), req_entry_size);
      if (output_txt) {
        if (typeid(T) == typeid(struct reqEntryReuseRealSystemTwrNS)) {
          auto re2 = reinterpret_cast<struct reqEntryReuseRealSystemTwrNS*>(&re);
          ofile_txt_ << re.get_real_time() << " " << re.obj_id_ << " " << re2->op_ << " " << re.next_access_vtime_ << "\n";
        } else {
          ofile_txt_ << re.get_real_time() << " " << re.obj_id_ << " " << re.next_access_vtime_ << "\n";
        }
      }
      n_req += 1;

      if (n_req % 100000000 == 0) {
        utilsPrint::print_time();
        std::cout << "write " << std::setprecision(4) << (double) n_req / 1000000 << " MReq" << std::endl;
      }
    }

    munmap(mapped_file, file_size);
    ofile_.close();
    if (output_txt)
      ofile_txt_.close();
  }
};
}







