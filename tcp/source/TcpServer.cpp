#include "../include/TcpServer.h"
#include <chrono>
#include <cstring>
#include <mutex>

using namespace stcp;


#ifdef _WIN32
#define WIN(exp) exp
#define NIX(exp)

inline _WinSocketIniter winsock_initer;

inline int convertError() {
    switch (WSAGetLastError()) {
    case 0:
        return 0;
    case WSAEINTR:
        return EINTR;
    case WSAEINVAL:
        return EINVAL;
    case WSA_INVALID_HANDLE:
        return EBADF;
    case WSA_NOT_ENOUGH_MEMORY:
        return ENOMEM;
    case WSA_INVALID_PARAMETER:
        return EINVAL;
    case WSAENAMETOOLONG:
        return ENAMETOOLONG;
    case WSAENOTEMPTY:
        return ENOTEMPTY;
    case WSAEWOULDBLOCK:
        return EAGAIN;
    case WSAEINPROGRESS:
        return EINPROGRESS;
    case WSAEALREADY:
        return EALREADY;
    case WSAENOTSOCK:
        return ENOTSOCK;
    case WSAEDESTADDRREQ:
        return EDESTADDRREQ;
    case WSAEMSGSIZE:
        return EMSGSIZE;
    case WSAEPROTOTYPE:
        return EPROTOTYPE;
    case WSAENOPROTOOPT:
        return ENOPROTOOPT;
    case WSAEPROTONOSUPPORT:
        return EPROTONOSUPPORT;
    case WSAEOPNOTSUPP:
        return EOPNOTSUPP;
    case WSAEAFNOSUPPORT:
        return EAFNOSUPPORT;
    case WSAEADDRINUSE:
        return EADDRINUSE;
    case WSAEADDRNOTAVAIL:
        return EADDRNOTAVAIL;
    case WSAENETDOWN:
        return ENETDOWN;
    case WSAENETUNREACH:
        return ENETUNREACH;
    case WSAENETRESET:
        return ENETRESET;
    case WSAECONNABORTED:
        return ECONNABORTED;
    case WSAECONNRESET:
        return ECONNRESET;
    case WSAENOBUFS:
        return ENOBUFS;
    case WSAEISCONN:
        return EISCONN;
    case WSAENOTCONN:
        return ENOTCONN;
    case WSAETIMEDOUT:
        return ETIMEDOUT;
    case WSAECONNREFUSED:
        return ECONNREFUSED;
    case WSAELOOP:
        return ELOOP;
    case WSAEHOSTUNREACH:
        return EHOSTUNREACH;
    default:
        return EIO;
    }
}


#else
#define WIN(exp)
#define NIX(exp) exp
#endif

TcpServer::TcpServer(const uint16_t port,
                     KeepAliveConfig ka_conf,
                     handler_function_t handler,
                     con_handler_function_t connect_hndl,
                     con_handler_function_t disconnect_hndl,
                     uint thread_count
                     )
  : port(port),
    handler(handler),
    connect_hndl(connect_hndl),
    disconnect_hndl(disconnect_hndl),
    thread_pool(thread_count),
    ka_conf(ka_conf)
    {}

TcpServer::~TcpServer() {
  if(_status == status::up)
    stop();
}

void TcpServer::setHandler(TcpServer::handler_function_t handler) {this->handler = handler;}

uint16_t TcpServer::getPort() const {return port;}
uint16_t TcpServer::setPort( const uint16_t port) {
	this->port = port;
	start();
	return port;
}

TcpServer::status TcpServer::start() {
  int flag;
  if(_status == status::up) stop();

  SocketAddr_in address;
  address.sin_addr
      WIN(.S_un.S_addr)NIX(.s_addr) = INADDR_ANY;
  address.sin_port = htons(port);
  address.sin_family = AF_INET;


  if((serv_socket = socket(AF_INET, SOCK_STREAM, 0)) WIN(== INVALID_SOCKET)NIX(== -1))
     return _status = status::err_socket_init;

  // Set nonblock accept
  NIX(
  if(int flags = fcntl(serv_socket, F_GETFL);
     fcntl(serv_socket, F_GETFL, flags | O_NONBLOCK) < 0) {
    return _status = status::err_socket_init;
  })

  // Bind address to socket
  if(flag = true;
     (setsockopt(serv_socket, SOL_SOCKET, SO_REUSEADDR, WIN((char*))&flag, sizeof(flag)) == -1) ||
     (bind(serv_socket, (struct sockaddr*)&address, sizeof(address)) WIN(== SOCKET_ERROR)NIX(< 0)))
     return _status = status::err_socket_bind;

  if(listen(serv_socket, SOMAXCONN) WIN(== SOCKET_ERROR)NIX(< 0))
    return _status = status::err_socket_listening;
  _status = status::up;
  thread_pool.addJob([this]{handlingAcceptLoop();});
  thread_pool.addJob([this]{waitingDataLoop();});
  return _status;
}

void TcpServer::stop() {
  _status = status::close;
  WIN(closesocket)NIX(close)(serv_socket);
  thread_pool.stop();
  client_list.clear();
}

void TcpServer::joinLoop() {thread_pool.join();}

bool TcpServer::connectTo(uint32_t host, uint16_t port, con_handler_function_t connect_hndl) {
  Socket client_socket;
  SocketAddr_in address;
  if((client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) WIN(== INVALID_SOCKET) NIX(< 0)) return false;

  new(&address) SocketAddr_in;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = host;
  WIN(address.sin_addr.S_un.S_addr = host;)
  NIX(address.sin_addr.s_addr = host;)

  address.sin_port = htons(port);

  if(connect(client_socket, (sockaddr *)&address, sizeof(address))
     WIN(== SOCKET_ERROR)NIX(!= 0)) {
    WIN(closesocket(client_socket);)NIX(close(client_socket);)
    return false;
  }

  if(!enableKeepAlive(client_socket)) {
    shutdown(client_socket, 0);
    WIN(closesocket)NIX(close)(client_socket);
  }

  std::unique_ptr<Client> client(new Client(client_socket, address));
  connect_hndl(*client);
  client_mutex.lock();
  client_list.emplace_back(std::move(client));
  client_mutex.unlock();
  return true;
}

void TcpServer::sendData(const void* buffer, const size_t size) {
  for(std::unique_ptr<Client>& client : client_list)
    client->sendData(buffer, size);
}

bool TcpServer::sendDataBy(uint32_t host, uint16_t port, const void* buffer, const size_t size) {
  bool data_is_sended = false;
  for(std::unique_ptr<Client>& client : client_list)
    if(client->getHost() == host &&
       client->getPort() == port) {
      client->sendData(buffer, size);
      data_is_sended = true;
    }
  return data_is_sended;
}

bool TcpServer::disconnectBy(uint32_t host, uint16_t port) {
  bool client_is_disconnected = false;
  for(std::unique_ptr<Client>& client : client_list)
    if(client->getHost() == host &&
       client->getPort() == port) {
      client->disconnect();
      client_is_disconnected = true;
    }
  return client_is_disconnected;
}

void TcpServer::disconnectAll() {
  for(std::unique_ptr<Client>& client : client_list)
    client->disconnect();
}

void TcpServer::handlingAcceptLoop() {
  SockLen_t addrlen = sizeof(SocketAddr_in);
  SocketAddr_in client_addr;
  if (Socket client_socket = accept(serv_socket, (struct sockaddr*)&client_addr, &addrlen);
      client_socket WIN(!= 0)NIX(>= 0) && _status == status::up) {

    // Enable keep alive for client
    if(enableKeepAlive(client_socket)) {
      std::unique_ptr<Client> client(new Client(client_socket, client_addr));
      connect_hndl(*client);
      client_mutex.lock();
      client_list.emplace_back(std::move(client));
      client_mutex.unlock();
    } else {
      shutdown(client_socket, 0);
      WIN(closesocket)NIX(close)(client_socket);
    }
  }

  if(_status == status::up)thread_pool.addJob([this](){handlingAcceptLoop();});
}

void TcpServer::waitingDataLoop() {
  {
    std::lock_guard lock(client_mutex);
    for(auto it = client_list.begin(), end = client_list.end(); it != end; ++it) {
      auto& client = *it;
      if(client){
        if(DataBuffer data = client->loadData(); !data.empty()) {

          thread_pool.addJob([this, _data = std::move(data), &client]{
            client->access_mtx.lock();
            handler(std::move(_data), *client);
            client->access_mtx.unlock();
          });
        } else if(client->_status == SocketStatus::disconnected) {

          thread_pool.addJob([this, &client, it]{
            client->access_mtx.lock();
            Client* pointer = client.release();
            client = nullptr;
            pointer->access_mtx.unlock();
            disconnect_hndl(*pointer);
            client_list.erase(it);
            delete pointer;
          });
        }
      }
    }
  }

  if(_status == status::up)thread_pool.addJob([this](){waitingDataLoop();});
}

bool TcpServer::enableKeepAlive(Socket socket) {
  int flag = 1;
#ifdef _WIN32
  tcp_keepalive ka {1, ka_conf.ka_idle * 1000, ka_conf.ka_intvl * 1000};
  if (setsockopt (socket, SOL_SOCKET, SO_KEEPALIVE, (const char *) &flag, sizeof(flag)) != 0) return false;
  unsigned long numBytesReturned = 0;
  if(WSAIoctl(socket, SIO_KEEPALIVE_VALS, &ka, sizeof (ka), nullptr, 0, &numBytesReturned, 0, nullptr) != 0) return false;
#else //POSIX
  if(setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag)) == -1) return false;
  if(setsockopt(socket, IPPROTO_TCP, TCP_KEEPIDLE, &ka_conf.ka_idle, sizeof(ka_conf.ka_idle)) == -1) return false;
  if(setsockopt(socket, IPPROTO_TCP, TCP_KEEPINTVL, &ka_conf.ka_intvl, sizeof(ka_conf.ka_intvl)) == -1) return false;
  if(setsockopt(socket, IPPROTO_TCP, TCP_KEEPCNT, &ka_conf.ka_cnt, sizeof(ka_conf.ka_cnt)) == -1) return false;
#endif
  return true;
}
