
#include <string>
#include <cstring>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "../../include/libCacheSim/logging.h"

namespace utils{
void *setup_mmap(const std::string &file_path, size_t *size) {
  int fd;
  struct stat st {};
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
    ERROR("Unable to allocate %llu bytes of memory, %s\n", (unsigned long long)st.st_size, strerror(errno));
    abort();
  }

#ifdef MADV_HUGEPAGE
  int mstatus = madvise(mapped_file, st.st_size, MADV_HUGEPAGE | MADV_SEQUENTIAL);
  if (mstatus != 0) {
    WARN("cannot turn on hugepage %s\n", strerror(errno));
  }
#endif

  close(fd);
  return mapped_file;
}

}