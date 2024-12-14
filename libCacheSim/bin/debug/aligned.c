
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "../../include/libCacheSim/reader.h"
#include "../../traceReader/customizedReader/lcs.h"

#define N_OP 200000000llu

void test_unaligned_read_mmap(char* filepath) {
  int fd = open(filepath, O_RDONLY);
  if (fd < 0) {
    perror("open");
    exit(1);
  }

  // get file size
  struct stat st;
  if (fstat(fd, &st) < 0) {
    perror("fstat");
    exit(1);
  }

  if (st.st_size < N_OP * 8) {
    fprintf(stderr, "file size is too small, require %llu bytes\n", N_OP * 8);
    exit(1);
  }

  void* data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (data == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }

  int64_t s = 0;
  double time_read_uint64_aligned = 0;
  double time_read_uint64_unaligned = 0;
  double time_read_uint32_aligned = 0;
  double time_read_uint32_unaligned = 0;
  double time_read_uint16_aligned = 0;
  double time_read_uint16_unaligned = 0;
  double time_read_uint8_aligned = 0;
  double time_read_uint8_unaligned = 0;

  char* ptr = (char*)data;
  struct timeval start, end;

  // warm up
  for (uint64_t i = 0; i < N_OP; i++) {
    uint64_t val = *(ptr + i * 8);
    s += val;
  }
  for (uint64_t i = 0; i < N_OP; i++) {
    uint64_t val = *(ptr + i * 8);
    s += val;
  }

  // benchmark read time of uint64_t aligned memory access
  gettimeofday(&start, NULL);
  for (uint64_t i = 0; i < N_OP; i++) {
    uint64_t val = *(ptr + i * 8);
    s += val;
  }
  gettimeofday(&end, NULL);
  time_read_uint64_aligned = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;

  // benchmark read time of uint64_t **unaligned** memory access
  gettimeofday(&start, NULL);
  for (uint64_t i = 0; i < N_OP; i++) {
    uint64_t val = *(ptr + i * 8 + 1);
    s += val;
  }
  gettimeofday(&end, NULL);
  time_read_uint64_unaligned = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;

  // benchmark read time of uint32_t aligned memory access
  gettimeofday(&start, NULL);
  for (uint64_t i = 0; i < N_OP; i++) {
    uint32_t val = *(uint32_t*)(ptr + i * 4);
    s += val;
  }
  gettimeofday(&end, NULL);
  time_read_uint32_aligned = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;

  // benchmark read time of uint32_t **unaligned** memory access
  gettimeofday(&start, NULL);
  for (uint64_t i = 0; i < N_OP; i++) {
    uint32_t val = *(uint32_t*)(ptr + i * 4 + 1);
    s += val;
  }
  gettimeofday(&end, NULL);
  time_read_uint32_unaligned = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;

  // benchmark read time of uint16_t aligned memory access
  gettimeofday(&start, NULL);
  for (uint64_t i = 0; i < N_OP; i++) {
    uint16_t val = *(uint16_t*)(ptr + i * 2);
    s += val;
  }
  gettimeofday(&end, NULL);
  time_read_uint16_aligned = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;

  // benchmark read time of uint16_t **unaligned** memory access
  gettimeofday(&start, NULL);
  for (uint64_t i = 0; i < N_OP; i++) {
    uint16_t val = *(uint16_t*)(ptr + i * 2 + 1);
    s += val;
  }
  gettimeofday(&end, NULL);
  time_read_uint16_unaligned = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;

  // benchmark read time of uint8_t aligned memory access
  gettimeofday(&start, NULL);
  for (uint64_t i = 0; i < N_OP; i++) {
    uint8_t val = *(ptr + i);
    s += val;
  }
  gettimeofday(&end, NULL);
  time_read_uint8_aligned = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;

  // benchmark read time of uint8_t **unaligned** memory access
  gettimeofday(&start, NULL);
  for (uint64_t i = 0; i < N_OP; i++) {
    uint8_t val = *(ptr + i + 1);
    s += val;
  }
  gettimeofday(&end, NULL);
  time_read_uint8_unaligned = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;

  printf("uint64_t   aligned read time: %.4f sec\n", time_read_uint64_aligned);
  printf("uint64_t unaligned read time: %.4f sec\n", time_read_uint64_unaligned);
  printf("uint32_t   aligned read time: %.4f sec\n", time_read_uint32_aligned);
  printf("uint32_t unaligned read time: %.4f sec\n", time_read_uint32_unaligned);
  printf("uint16_t   aligned read time: %.4f sec\n", time_read_uint16_aligned);
  printf("uint16_t unaligned read time: %.4f sec\n", time_read_uint16_unaligned);
  printf("uint8_t    aligned read time: %.4f sec\n", time_read_uint8_aligned);
  printf("uint8_t  unaligned read time: %.4f sec\n", time_read_uint8_unaligned);
  printf("s: %ld\n", s);

  close(fd);
}

// struct aligned_struct1 {
//   uint8_t d;
//   uint16_t c;
//   uint32_t b;
//   uint64_t a;
// };

// struct __attribute__((packed)) unaligned_struct1 {
//   uint8_t d;
//   uint16_t c;
//   uint32_t b;
//   uint64_t a;
// };



struct aligned_struct2 {
  uint32_t a;
  uint64_t b;
  uint32_t c;
  uint64_t d;
};

struct __attribute__((packed)) unaligned_struct2 {
  uint32_t a;
  uint64_t b;
  uint32_t c;
  uint64_t d;
};


struct aligned_struct {
  uint32_t a;
  uint64_t b;
  uint32_t c;
  uint64_t d;
};

struct __attribute__((packed)) unaligned_struct {
  uint32_t a;
  uint64_t b1:8;
  uint64_t b2:16;
  uint64_t b3:32;
  uint64_t b4:8;
  uint32_t c;
  uint64_t d;
};
// struct __attribute__((packed)) unaligned_struct {
//   uint32_t b1;
//   uint64_t b2;
//   uint32_t b3;
//   uint64_t b4;
// };

void test_unaligned_read_mmap_struct(char* filepath) {
  printf("aligned struct size: %lu\n", sizeof(struct aligned_struct));
  printf("unaligned struct size: %lu\n", sizeof(struct unaligned_struct));

  int fd = open(filepath, O_RDONLY);
  if (fd < 0) {
    perror("open");
    exit(1);
  }

  // get file size
  struct stat st;
  if (fstat(fd, &st) < 0) {
    perror("fstat");
    exit(1);
  }

  if (st.st_size < N_OP * sizeof(struct aligned_struct)) {
    fprintf(stderr, "file size is too small\n");
    exit(1);
  }

  void* data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (data == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }

  int64_t s = 0;
  double time_read_struct_aligned = 0;
  double time_read_struct_unaligned = 0;

  char* ptr = (char*)data;
  struct timeval start, end;

  // warm up
  for (uint64_t i = 0; i < N_OP; i++) {
    struct aligned_struct val = *(struct aligned_struct *)(ptr + i * sizeof(struct aligned_struct));
    s += val.a + val.b + val.c + val.d;
  }

  // benchmark read time of struct aligned memory access
  gettimeofday(&start, NULL);
  for (uint64_t i = 0; i < N_OP; i++) {
    struct aligned_struct val = *(struct aligned_struct *)(ptr + i * sizeof(struct aligned_struct));
    s += val.a + val.b + val.c + val.d;
  }
  gettimeofday(&end, NULL);
  time_read_struct_aligned = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;

  // benchmark read time of struct **unaligned** memory access
  gettimeofday(&start, NULL);
  for (uint64_t i = 0; i < N_OP; i++) {
    struct unaligned_struct val = *(struct unaligned_struct *)(ptr + i * sizeof(struct unaligned_struct));
    // s += val.a + val.b + val.c + val.d;
    s += val.b1 + val.b2 + val.b3 + val.b4;
  }
  gettimeofday(&end, NULL);
  time_read_struct_unaligned = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;

  printf("struct   aligned read time: %.4f sec\n", time_read_struct_aligned);
  printf("struct unaligned read time: %.4f sec\n", time_read_struct_unaligned);

  printf("s: %ld\n", s);
  close(fd);
}

void test_unaligned_read_struct(char* filepath) {
  FILE* fp = fopen(filepath, "rb");
  if (fp == NULL) {
    perror("fopen");
    exit(1);
  }

  int64_t s = 0;
  double time_read_struct_aligned = 0;
  double time_read_struct_unaligned = 0;

  struct aligned_struct aligned;
  struct unaligned_struct unaligned;

  struct timeval start, end;

  // warm up
  for (uint64_t i = 0; i < N_OP; i++) {
    int n = fread(&aligned, sizeof(struct aligned_struct), 1, fp);
    s += aligned.a + aligned.b + aligned.c + aligned.d;
  }

  // benchmark read time of struct aligned memory access
  fseek(fp, 0, SEEK_SET);
  gettimeofday(&start, NULL);
  for (uint64_t i = 0; i < N_OP; i++) {
    int n = fread(&aligned, sizeof(struct aligned_struct), 1, fp);
    s += aligned.a + aligned.b + aligned.c + aligned.d;
  }
  gettimeofday(&end, NULL);
  time_read_struct_aligned = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;

  // benchmark read time of struct **unaligned** memory access
  fseek(fp, 0, SEEK_SET);
  gettimeofday(&start, NULL);
  for (uint64_t i = 0; i < N_OP; i++) {
    int n = fread(&unaligned, sizeof(struct unaligned_struct), 1, fp);
    // s += unaligned.a + unaligned.b + unaligned.c + unaligned.d;
    s += unaligned.b1 + unaligned.b2 + unaligned.b3 + unaligned.b4;
  }
  gettimeofday(&end, NULL);
  time_read_struct_unaligned = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;

  printf("struct   aligned read time: %.4f sec\n", time_read_struct_aligned);
  printf("struct unaligned read time: %.4f sec\n", time_read_struct_unaligned);

  printf("s: %ld\n", s);
}

int main(int argc, char* argv[]) {
  printf("lcs_req_v1_t size: %lu\n", sizeof(lcs_req_v1_t));
  printf("lcs_req_v2_t size: %lu\n", sizeof(lcs_req_v2_t));
  printf("lcs_trace_stat size: %lu\n", sizeof(lcs_trace_stat_t));
  printf("lcs_trace_header size: %lu\n", sizeof(lcs_trace_header_t));

  if (argc < 2) {
    printf("Usage: %s <trace_file>\n", argv[0]);
    return 1;
  }

  test_unaligned_read_mmap(argv[1]);
  test_unaligned_read_mmap_struct(argv[1]);
  test_unaligned_read_struct(argv[1]);

  return 0;
}