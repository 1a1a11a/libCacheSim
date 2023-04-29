
#include "include/threadPool.h"
#include "include/utilsSys.h"

utilsSys::ThreadPool::ThreadPool(int n_thread) : n_set_thread_(n_thread) {
  if (n_set_thread_ == -1) {
    n_set_thread_ = get_n_cores();
  }

  for (int i = 0; i < n_set_thread_; i++) {
    pool_.emplace_back(&ThreadPool::wait_job, this);
  }
}

void utilsSys::ThreadPool::shutdown() {
  std::unique_lock<std::mutex> lock(mtx_);
  if (stopped_)
    return;

  printf("thread pool shutdown now\n");
  stopped_ = true; // use this flag in condition.wait
  lock.unlock();

  cond_.notify_all(); // wake up all threads.

  // Join all threads.
  for(std::thread &th : pool_) {
    th.join();
  }

  pool_.clear();
}

void utilsSys::ThreadPool::wait_job() {
  while (true) {
    std::unique_lock<std::mutex> lock(mtx_);

    cond_.wait(lock, [this](){
      return !job_queue.empty() || stopped_;
    });

    if (stopped_)
      return;

    auto job = job_queue.front();
    n_running_jobs.fetch_add(1);
    job_queue.pop();

    while (n_set_thread_ == -1 && get_n_available_cores() <= 1) {
      /* only run when the load is not more than cores */
      printf("throttle due to high load\n");
      std::this_thread::sleep_for(std::chrono::seconds (8));
    }

    job();
    n_running_jobs.fetch_sub(1);
  }
};

void utilsSys::ThreadPool::add_Job(const std::function<void()>& new_job) {
  std::unique_lock<std::mutex> lock(mtx_);
  job_queue.push(new_job);
  lock.unlock();
  cond_.notify_one();
}

void utilsSys::ThreadPool::wait_for_finish() {
  while (true) {
    std::unique_lock<std::mutex> lock(mtx_);
    if (job_queue.empty() && n_running_jobs == 0) {
      return;
    }
    lock.unlock();
    std::this_thread::sleep_for(std::chrono::microseconds (20 + job_queue.size() * 100));
  }
}