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
#else // POSIX
typedef socklen_t SockLen_t;
typedef struct sockaddr_in SocketAddr_in;
typedef int Socket;
#endif

struct KeepAliveConfig{
  int ka_idle = 120;
  int ka_intvl = 3;
  int ka_cnt = 5;
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
  std::list<Client> client_list;
  std::shared_mutex client_mutex;
  std::list<std::thread> client_handler_threads;
  std::mutex client_handler_mutex;

#ifdef _WIN32 // Windows NT
  WSAData w_data;
#endif

  void handlingLoop();
  bool enableKeepAlive(Socket socket);
  void clientHandler(std::list<Client>::iterator cur_client);

public:
  TcpServer(const uint16_t port,
            handler_function_t handler);
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
  SocketAddr_in address;
  Socket socket;
  status _status = status::connected;

  DataBuffer loadData();
public:
  Client(Socket socket, SocketAddr_in address);
  Client(Client&& other);
  ~Client();
  uint32_t getHost() const;
  uint16_t getPort() const;

  bool sendData(const char* buffer, const size_t size) const;
};

/*

  *NIX

#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

int socket_set_keepalive (int fd) {
  int ret, error, flag, alive, idle, cnt, intv;
  / * Set: использовать keepalive на fd * /
  alive = 1;
  if (setsockopt of live, alive, SOL_SOCKET, LIVE, SOL_SOCKET, SO )! = 0)
    log_warn ("Set keepalive error:% s. \ N", strerror (errno)); return -1;
  / * Нет данных в течение 10 секунд, запустить механизм поддержки активности, отправить пакет keepalive * /
  idle = 10;
  if (setsockopt (fd, SOL_TCP, TCP_KEEPIDLE, & idle, sizeof idle)! = 0)
    log_warn ("Установить ошибку ожидания keepalive:% s. \ n", strerror (errno));
  / * Если ответ не получен, пакет поддержки активности будет повторно отправлен через 5 секунд * /
  intv = 5;
  if (setsockopt (fd, SOL_TCP, TCP_KEEPINTVL, & intv, sizeof intv)! = 0)
    log_warn ("Set keepalive intv error:% s. \ n ", strerror (errno));
  / * если * /
  if(setsockopt (fd, SOL_TCP, TCP_KEEPCNT)
  / * не получает пакет keep-alive 3 раза подряд, это рассматривается как сбой соединения * /
  cnt = 3;
  if (setsockopt (fd, SOL_TCP, TCP_KEEPCNT) , & cnt, sizeof cnt)! = 0)
    log_warn ("Set keepalive cnt error:% s. \ n", strerror (errno));
}

// errno == ECONNRESET || errno == ETIMEDOUT


  WINDOWS

#include <winsock2.h>
#include <mstcpip.h>

int socket_set_keepalive (int fd)  {

  struct tcp_keepalive kavars[1] = {
    1,
    10 * 1000, // 10 seconds
    5 * 1000 //5 seconds
  };
  // Set: use keepalive on fd
  alive = 1;
  if (setsockopt (fd, SOL_SOCKET, SO_KEEPALIVE, (const char *) &alive, sizeof alive) != 0) {
    log_warn ("Set keep alive error: %s.\n", strerror (errno));
    return -1;
  }
  if(WSAIoctl(fd, SIO_KEEPALIVE_VALS, kavars, sizeof kavars, NULL, sizeof (int), &ret, NULL, NULL) != 0) {
    log_warn ("Set keep alive error: %s.\n", strerror (WSAGetLastError ()));
    return -1;
  }
  return 0;
}

// int errno = WSAGetLastError(); errno == WSAECONNRESET || errno == WSAETIMEDOUT

*/

#endif // TCPSERVER_H
