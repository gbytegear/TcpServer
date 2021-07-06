#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <functional>
#include <list>

#include <thread>
#include <mutex>
#include <shared_mutex>

#ifdef _WIN32 // Windows NT

#include <WinSock2.h>
#include <mstcpip.h>

#else // *nix

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#endif

#include "general.h"

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

struct KeepAliveConfig{
  ka_prop_t ka_idle = 120;
  ka_prop_t ka_intvl = 3;
  ka_prop_t ka_cnt = 5;
};

struct TcpServer {
  struct Client;
  class ClientList;
  typedef std::function<void(DataBuffer, Client&)> handler_function_t;
  enum class status : uint8_t {
    up = 0,
    err_socket_init = 1,
    err_socket_bind = 2,
    err_scoket_keep_alive = 3,
    err_socket_listening = 4,
    close = 5
  };

private:
  Socket serv_socket;
  uint16_t port;
  status _status = status::close;
  handler_function_t handler;
  std::thread handler_thread;

  KeepAliveConfig ka_conf;

  // Client & Client handling
  std::list<std::unique_ptr<Client>> client_list;
  std::shared_mutex client_mutex;
  std::list<std::thread> client_handler_threads;
  std::mutex client_handler_mutex;

#ifdef _WIN32 // Windows NT
  WSAData w_data;
#endif

  void handlingLoop();
  bool enableKeepAlive(Socket socket);
  void clientHandler(std::list<std::unique_ptr<Client>>::iterator cur);

public:
  TcpServer(const uint16_t port,
            handler_function_t handler,
            KeepAliveConfig ka_conf = {});
  ~TcpServer();

  //! Set client handler
  void setHandler(handler_function_t handler);

  uint16_t getPort() const;
  uint16_t setPort(const uint16_t port);

  status getStatus() const {return _status;}

  status start();
  void stop();

  void joinLoop();
};

struct TcpServer::Client {

  typedef SocketStatus status;

private:
  friend struct TcpServer;

  std::mutex access_mtx;
  std::mutex move_next_mtx;
  SocketAddr_in address;
  Socket socket;
  status _status = status::connected;

  DataBuffer loadData();
  void disconnect();


public:
  Client(Socket socket, SocketAddr_in address);
  ~Client();
  uint32_t getHost() const;
  uint16_t getPort() const;

  bool sendData(const char* buffer, const size_t size) const;
};

#endif // TCPSERVER_H
