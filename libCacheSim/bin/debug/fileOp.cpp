
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>

typedef struct __attribute__((packed)) req1 {
  int64_t clock_time;
  uint64_t obj_id;
  int64_t obj_size;
  uint32_t op : 8;
  uint32_t tenant : 24;
  int64_t next_access_vtime;
} req1_t;

typedef struct __attribute__((packed)) req2 {
  int64_t clock_time;
  uint64_t obj_id;
  int64_t obj_size;
  int8_t op;
  uint16_t tenant;
  int64_t next_access_vtime;
} req2_t;

typedef req2_t req_t;

void write_req(const char *file_path, req_t *req) {
  std::ofstream file(file_path, std::ios::out | std::ios::binary | std::ios::trunc);
  file.write(reinterpret_cast<char *>(req), sizeof(req_t));
}

void read_req(const char *file_path, req_t *req) {
  std::ifstream file(file_path, std::ios::in | std::ios::binary);
  file.read(reinterpret_cast<char *>(req), sizeof(req_t));
}

void print_req(req_t *req) {
  std::cout << "clock_time: " << req->clock_time << std::endl;
  std::cout << "obj_id: " << req->obj_id << std::endl;
  std::cout << "obj_size: " << req->obj_size << std::endl;
  std::cout << "op: " << (int)(req->op) << std::endl;
  std::cout << "tenant: " << req->tenant << std::endl;
  std::cout << "next_access_vtime: " << req->next_access_vtime << std::endl;
}

int main() {
  req_t req, req2;
  req.clock_time = 123456789;
  req.obj_id = 987654321;
  req.obj_size = 123456789;
  req.op = 1;
  req.tenant = 2;
  req.next_access_vtime = 987654321;

  print_req(&req);
  write_req("req.bin", &req);

  read_req("req.bin", &req2);
  print_req(&req2);

  return 0;
}
