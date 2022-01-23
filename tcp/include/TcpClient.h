#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include <cstdint>
#include <cstddef>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
typedef SOCKET socket_t;
#else
typedef int socket_t;
//#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#endif

#include "general.h"
#include <memory.h>

/// Sipmple TCP
namespace stcp {

#define NONBLOCK

struct TcpClient : public TcpClientBase {
  SocketAddr_in address;
  socket_t client_socket;

  std::mutex handle_mutex;
  std::function<void(DataBuffer)> handler_func = [](DataBuffer){};
  std::unique_ptr<std::thread> handler_thread;
#ifdef _WIN32
  WSAData w_data;
#else
#endif
  status _status = status::disconnected;
public:
  typedef std::function<void(DataBuffer)> handler_function_t;
  TcpClient() noexcept;
  virtual ~TcpClient() override;

  status connectTo(uint32_t host, uint16_t port) noexcept;
  virtual status disconnect() noexcept override;

  virtual uint32_t getHost() const override;
  virtual uint16_t getPort() const override;
  virtual status getStatus() const override {return _status;}

  virtual DataBuffer loadData() override;
  std::thread& setHandler(handler_function_t handler);
  void joinHandler();

  virtual bool sendData(const void* buffer, const size_t size) const override;
  virtual SocketType getType() const override {return SocketType::client_socket;}
};

}

#endif // TCPCLIENT_H
