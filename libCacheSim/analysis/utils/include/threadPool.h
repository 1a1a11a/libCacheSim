#pragma once

#include <thread>
#include <iostream>
#include <mutex>
#include <vector>
#include <condition_variable>
#include <queue>
#include <functional>
#include <chrono>
#include <atomic>

//#include "utilsSys.h"


namespace utilsSys {

class ThreadPool {
 public:

  explicit ThreadPool(int n_thread = -1);
  ~ThreadPool() { shutdown(); }

  void shutdown();
  void add_Job(const std::function<void()> &new_job);
  void wait_for_finish();

  int n_set_thread_;

 private:
  std::queue<std::function<void()>> job_queue;
  std::vector<std::thread> pool_;
  std::mutex mtx_;
  std::condition_variable cond_;
  bool stopped_ = false;
  std::atomic<int> n_running_jobs = 0;

  void wait_job();

};
}