#include "../include/TcpServer.h"
#include <chrono>
#include <cstring>
#include <mutex>

#ifdef _WIN32
#define WIN(exp) exp
#define NIX(exp)

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
                     handler_function_t handler,
                     KeepAliveConfig ka_conf)
  : TcpServer(port, handler, [](Client&){}, [](Client&){}, ka_conf) {}

TcpServer::TcpServer(const uint16_t port,
                     handler_function_t handler,
                     con_handler_function_t connect_hndl,
                     con_handler_function_t disconnect_hndl,
                     KeepAliveConfig ka_conf)
  : port(port), handler(handler), connect_hndl(connect_hndl), disconnect_hndl(disconnect_hndl), ka_conf(ka_conf) {}

TcpServer::~TcpServer() {
  if(_status == status::up)
	stop();
	WIN(WSACleanup());
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

  WIN(if(WSAStartup(MAKEWORD(2, 2), &w_data) == 0) {})
  SocketAddr_in address;
  address.sin_addr
      WIN(.S_un.S_addr)NIX(.s_addr) = INADDR_ANY;
  address.sin_port = htons(port);
  address.sin_family = AF_INET;


  if((serv_socket = socket(AF_INET, SOCK_STREAM, 0)) WIN(== INVALID_SOCKET)NIX(== -1))
     return _status = status::err_socket_init;

  flag = true;
  if((setsockopt(serv_socket, SOL_SOCKET, SO_REUSEADDR, WIN((char*))&flag, sizeof(flag)) == -1) ||
     (bind(serv_socket, (struct sockaddr*)&address, sizeof(address)) WIN(== SOCKET_ERROR)NIX(< 0)))
     return _status = status::err_socket_bind;

  if(listen(serv_socket, SOMAXCONN) WIN(== SOCKET_ERROR)NIX(< 0))
    return _status = status::err_socket_listening;
  _status = status::up;
  handler_thread = std::thread([this]{handlingLoop();});
  return _status;
}

void TcpServer::stop() {
  _status = status::close;
  WIN(closesocket)NIX(close)(serv_socket);

  joinLoop();
  client_handler_mutex.lock();
  for(std::thread& cl_thr : client_handler_threads)
    cl_thr.join();
  client_handler_mutex.unlock();
  client_handler_threads.clear();
  client_list.clear();
}

void TcpServer::joinLoop() {handler_thread.join();}

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

void TcpServer::handlingLoop() {
  SockLen_t addrlen = sizeof(SocketAddr_in);
  while (_status == status::up) {
    SocketAddr_in client_addr;
    if (Socket client_socket = accept(serv_socket, (struct sockaddr*)&client_addr, &addrlen);
        client_socket WIN(!= 0)NIX(>= 0) && _status == status::up) {

      if(client_socket == WIN(INVALID_SOCKET)NIX(-1)) continue;

      // Enable keep alive for client
      if(!enableKeepAlive(client_socket)) {
        shutdown(client_socket, 0);
        WIN(closesocket)NIX(close)(client_socket);
      }

      std::unique_ptr<Client> client(new Client(client_socket, client_addr));
      connect_hndl(*client);
      client_mutex.lock();
      client_list.emplace_back(std::move(client));
      client_mutex.unlock();
      if(client_handler_threads.empty())
        client_handler_threads.emplace_back(std::thread([this]{clientHandler(client_list.begin());}));
    }
  }

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


void TcpServer::clientHandler(ClientIterator cur) {
  const std::thread::id this_thr_id = std::this_thread::get_id();

  auto moveToNextClient = [this, &cur]()->bool{
    std::mutex& move_next_mtx = (*cur)->move_next_mtx;
    if(!*cur) return false;
    move_next_mtx.lock();
    if(++cur == client_list.end()) {
      cur = client_list.begin();
      if(cur == client_list.end()) { // if list is empty
        move_next_mtx.unlock();
        return false;
      }
    }
    move_next_mtx.unlock();
    return true;
  };


  auto removeThread = [&this_thr_id, this]() {
    client_handler_mutex.lock();
    for(auto it = client_handler_threads.begin(),
        end = client_handler_threads.end();
        it != end ;++it) if(it->get_id() == this_thr_id) {
      it->detach();
      client_handler_threads.erase(it);
      client_handler_mutex.unlock();
      return;
    }
    client_handler_mutex.unlock();
  };

  auto createThread = [this, &cur]() mutable {
    client_handler_mutex.lock();
    client_handler_threads.emplace_back([this, cur]() mutable {
      client_mutex.lock_shared();
      auto it = (++cur == client_list.end())? client_list.begin() : cur;
      client_mutex.unlock_shared();
      clientHandler(it);
    });
    client_handler_mutex.unlock();
  };


  while(_status == status::up) {

    if(!*cur) { // If client element is remove
      if(moveToNextClient()) continue;
      else { removeThread(); return; }
    } else if(!(*cur)->access_mtx.try_lock()) { // If client is handle now from other thread
      if(moveToNextClient()) continue;
      else { removeThread(); return; }
    }

    if(DataBuffer data = (*cur)->loadData(); data.size) {
      createThread();

      handler(data, **cur);

      (*cur)->access_mtx.unlock();
      removeThread();
      return;

    } else {

      // Remove disconnected client
      if((*cur)->_status != SocketStatus::connected) {
        disconnect_hndl(**cur);
        client_mutex.lock();

        // If client_list.length() == 1;
        if(client_list.begin() == --client_list.end()) {
          (*cur)->access_mtx.unlock();
          client_list.erase(cur);
          removeThread();
          client_mutex.unlock();
          return;
        }

        auto prev = cur;
        auto last_cur = cur;
        if(prev == client_list.begin()) prev = --client_list.end();
        else --prev;

        bool move_res = moveToNextClient();
        (*prev)->move_next_mtx.lock();
        (*last_cur)->access_mtx.unlock();
        client_list.erase(last_cur);
        (*prev)->move_next_mtx.unlock();

        client_mutex.unlock();
        if(move_res) continue;
        else {removeThread(); return;}
        return;
      }

      // Move to next client
      (*cur)->access_mtx.unlock();
      if(moveToNextClient()) continue;
      else {removeThread(); return;}
    }

  }


}
