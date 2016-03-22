#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <deque>
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace ThreadPool {

class Job {
public:
  Job () {}

  virtual ~Job () {}

  virtual void run ( void * ) = 0;
};

template <typename T>
class BlockingQueue {
public:
  BlockingQueue () 
    : m_lock(m_mutex, std::defer_lock)
    {}

  ~BlockingQueue () {}

  T pop () {
    std::lock_guard<std::mutex> lg(m_mutex);
    while ( m_queue.empty() ) {
      m_condvar.wait(m_lock);
    }

    const T temp = m_queue.front();
    m_queue.pop_front();

    return temp;
  }

  void push ( const T& value ) {
    {
      std::lock_guard<std::mutex> lg(m_mutex);
      m_queue.push_back(value);
    }

    m_condvar.notify_one();
  }

private:
  std::mutex m_mutex;
  std::unique_lock<std::mutex> m_lock;
  std::condition_variable m_condvar;

  std::deque<T> m_queue;
};

void * start_thread ( void * arg );

class ThreadPool {
public:
  ThreadPool ( const size_t thread_num = 2 ) {
      m_threads.reserve(thread_num);
      for ( size_t i=0; i<thread_num; ++i ) {
        m_threads.push_back(std::thread(start_thread, this));
      }
    }

  ~ThreadPool () {}

  void * execute_thread () {
    while ( true ) {
      const std::shared_ptr<Job>& job = m_jobs.pop();
      job->run(NULL);
    }

    return NULL;
  }

  void run ( const std::shared_ptr<Job>& job ) {
    m_jobs.push(job);
  }

private:
  std::vector<std::thread> m_threads;

  BlockingQueue<std::shared_ptr<Job> > m_jobs;
};

void * start_thread ( void * arg ) {
  ThreadPool * tp = (ThreadPool*) arg;
  return tp->execute_thread();
}

} // namespace ThreadPool

#endif
