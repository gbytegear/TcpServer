#ifndef GENERAL_H
#define GENERAL_H

#ifdef _WIN32
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
  disconnected = 4
};

/**
 * @brief Simple thread pool implementation
 */
class ThreadPool {
public:
  typedef std::function<void()> Job;
private:
  std::queue<Job> job_queue;
  std::mutex queue_mtx;
  std::condition_variable condition;
  std::vector<std::thread> thread_pool;
  bool pool_terminate = false;

  void setupThreadPool(uint thread_count) {
    thread_pool.clear();
    for(uint i = 0; i < thread_count; ++i) thread_pool.emplace_back([this](){workerLoop();});
  }

  void workerLoop() {
    while (true) {
      Job job;
      {
        std::unique_lock lock(queue_mtx);
        condition.wait(lock, [this](){return !job_queue.empty() || pool_terminate;});
        if(pool_terminate) return;
        job = job_queue.front();
        job_queue.pop();
      }
      job();
    }
  }
public:
  ThreadPool(unsigned thread_count = std::thread::hardware_concurrency()) {
    setupThreadPool(thread_count);
  }

  void addJob(Job job) {
    {
      std::unique_lock lock(queue_mtx);
      job_queue.push(std::move(job));
    }
    condition.notify_one();
  }

  void join() {for(auto& thread : thread_pool) thread.join();}

  uint getThreadCount() const {return thread_pool.size();}

  void stop() {
    pool_terminate = true;
    join();
    // Clear jobs in queue
    std::queue<Job> empty;
    std::swap(job_queue, empty);
    // reset thread pool
    setupThreadPool(thread_pool.size());
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

}

#endif // GENERAL_H
