#ifndef GENERAL_H
#define GENERAL_H

#ifdef _WIN32
#include <WinSock2.h>
#else
#define SD_BOTH 0
#include <sys/socket.h>
#endif


#include <cstdint>
#include <cstring>
#include <cinttypes>
#include <malloc.h>

#include <queue>
#include <vector>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

/// Sipmple TCP
namespace stcp {

#ifdef _WIN32 // Windows NT
typedef int SockLen_t;
typedef SOCKADDR_IN SocketAddr_in;
typedef SOCKET Socket;
typedef u_long ka_prop_t;
#else // POSIX
typedef socklen_t SockLen_t;
typedef struct sockaddr_in SocketAddr_in;
typedef int Socket;
typedef int ka_prop_t;
#endif

constexpr uint32_t LOCALHOST_IP = 0x0100007f;

enum class SocketStatus : uint8_t {
  connected = 0,
  err_socket_init = 1,
  err_socket_bind = 2,
  err_socket_connect = 3,
  disconnected = 4,
};

/// Simple thread pool implementation
class ThreadPool {
  std::vector<std::thread> thread_pool;
  std::queue<std::function<void()>> job_queue;
  std::mutex queue_mtx;
  std::condition_variable condition;
  std::atomic<bool> pool_terminated = false;

  void setupThreadPool(unsigned int thread_count) {
    thread_pool.clear();
    for(unsigned int i = 0; i < thread_count; ++i)
      thread_pool.emplace_back(&ThreadPool::workerLoop, this);
  }

  void workerLoop() {
    std::function<void()> job;
    while (!pool_terminated) {
      {
        std::unique_lock lock(queue_mtx);
        condition.wait(lock, [this](){return !job_queue.empty() || pool_terminated;});
        if(pool_terminated) return;
        job = job_queue.front();
        job_queue.pop();
      }
      job();
    }
  }
public:
  ThreadPool(unsigned int thread_count = std::thread::hardware_concurrency()) {setupThreadPool(thread_count);}

  ~ThreadPool() {
    pool_terminated = true;
    join();
  }

  template<typename F>
  void addJob(F job) {
    if(pool_terminated) return;
    {
      std::unique_lock lock(queue_mtx);
      job_queue.push(std::function<void()>(job));
    }
    condition.notify_one();
  }

  template<typename F, typename... Arg>
  void addJob(const F& job, const Arg&... args) {addJob([job, args...]{job(args...);});}

  void join() {for(auto& thread : thread_pool) thread.join();}

  unsigned int getThreadCount() const {return thread_pool.size();}

  void dropUnstartedJobs() {
    pool_terminated = true;
    join();
    pool_terminated = false;
    // Clear jobs in queue
    std::queue<std::function<void()>> empty;
    std::swap(job_queue, empty);
    // reset thread pool
    setupThreadPool(thread_pool.size());
  }

  void stop() {
    pool_terminated = true;
    join();
  }

  void start(unsigned int thread_count = std::thread::hardware_concurrency()) {
    if(!pool_terminated) return;
    pool_terminated = false;
    setupThreadPool(thread_count);
  }

};

typedef std::vector<uint8_t> DataBuffer;

enum class SocketType : uint8_t {
  client_socket = 0,
  server_socket = 1
};

class TcpClientBase {
public:
  typedef SocketStatus status;
  virtual ~TcpClientBase() {};
  virtual status disconnect() = 0;
  virtual status getStatus() const = 0;
  virtual bool sendData(const void* buffer, const size_t size) const = 0;
  virtual DataBuffer loadData() = 0;
  virtual uint32_t getHost() const = 0;
  virtual uint16_t getPort() const = 0;
  virtual SocketType getType() const = 0;
};

#ifdef _WIN32 // Windows NT
namespace {

class _WinSocketIniter {
  static inline WSAData w_data;
public:
  _WinSocketIniter() { WSAStartup(MAKEWORD(2, 2), &w_data); }
  ~_WinSocketIniter() { WSACleanup(); }
};

static inline _WinSocketIniter _winsock_initer;
}
#endif

}

#endif // GENERAL_H
