#include "../include/TcpClient.h"
#include <stdio.h>
#include <cstring>
#include <iostream>

using namespace stcp;

#ifdef _WIN32
#define WIN(exp) exp
#define NIX(exp)
#define WINIX(win_exp, nix_exp) win_exp


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
#define WINIX(win_exp, nix_exp) nix_exp
#endif


void TcpClient::handleSingleThread() {
  try {
    while(_status == status::connected) {
      if(DataBuffer data = loadData(); !data.empty()) {
        std::lock_guard lock(handle_mutex);
        handler_func(std::move(data));
      } else if (_status != status::connected) return;
    }
  }  catch (std::exception& except) {
    std::cerr << except.what() << std::endl;
    return;
  }
}

void TcpClient::handleThreadPool() {
  try {
    if(DataBuffer data = loadData(); !data.empty()) {
      std::lock_guard lock(handle_mutex);
      handler_func(std::move(data));
    }
    if(_status == status::connected) threads.thread_pool->addJob([this]{handleThreadPool();});
  } catch (std::exception& except) {
    std::cerr << except.what() << std::endl;
    return;
  } catch (...) {
    std::cerr << "Unhandled exception!" << std::endl;
    return;
  }
}

TcpClient::TcpClient() noexcept : _status(status::disconnected) {}
TcpClient::TcpClient(ThreadPool* thread_pool) noexcept :
  thread_management_type(ThreadManagementType::thread_pool),
  threads(thread_pool),
  _status(status::disconnected) {}

TcpClient::~TcpClient() {
  disconnect();
  WIN(WSACleanup();)

      switch (thread_management_type) {
    case stcp::TcpClient::ThreadManagementType::single_thread:
      if(threads.thread) threads.thread->join();
      delete threads.thread;
    break;
    case stcp::TcpClient::ThreadManagementType::thread_pool: break;
  }
}

TcpClient::status TcpClient::connectTo(uint32_t host, uint16_t port) noexcept {
  WIN(if(WSAStartup(MAKEWORD(2, 2), &w_data) != 0) {})

  if((client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) WIN(== INVALID_SOCKET) NIX(< 0)) return _status = status::err_socket_init;

  new(&address) SocketAddr_in;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = host;
  WINIX(
      address.sin_addr.S_un.S_addr = host;
      ,
      address.sin_addr.s_addr = host;
  )
  address.sin_port = htons(port);

  if(connect(client_socket, (sockaddr *)&address, sizeof(address))
     WINIX(== SOCKET_ERROR,!= 0)
     ) {
    WINIX(closesocket(client_socket); ,close(client_socket);)
		return _status = status::err_socket_connect;
	}
	return _status = status::connected;
}

TcpClient::status TcpClient::disconnect() noexcept {
	if(_status != status::connected)
		return _status;
  _status = status::disconnected;
  switch (thread_management_type) {
    case stcp::TcpClient::ThreadManagementType::single_thread:
      if(threads.thread) threads.thread->join();
      delete threads.thread;
    break;
    case stcp::TcpClient::ThreadManagementType::thread_pool: break;
  }
  shutdown(client_socket, SD_BOTH);
  WINIX(closesocket(client_socket), close(client_socket));
  return _status;
}

DataBuffer TcpClient::loadData() {
    DataBuffer buffer;
    uint32_t size;
    int err;

    // Read data length in non-blocking mode
    // MSG_DONTWAIT - Unix non-blocking read
    WIN(if(u_long t = true; SOCKET_ERROR == ioctlsocket(client_socket, FIONBIO, &t)) return DataBuffer();) // Windows non-blocking mode on
    int answ = recv(client_socket, (char*)&size, sizeof (size), NIX(MSG_DONTWAIT)WIN(0));
    WIN(if(u_long t = false; SOCKET_ERROR == ioctlsocket(client_socket, FIONBIO, &t)) return DataBuffer();) // Windows non-blocking mode off

    // Disconnect
    if(!answ) {
      disconnect();
      return DataBuffer();
    } else if(answ == -1) {
      // Error handle (f*ckin OS-dependence!)
      WIN(
        err = convertError();
        if(!err) {
          SockLen_t len = sizeof (err);
          getsockopt (client_socket, SOL_SOCKET, SO_ERROR, WIN((char*))&err, &len);
        }
      )NIX(
        SockLen_t len = sizeof (err);
        getsockopt (client_socket, SOL_SOCKET, SO_ERROR, WIN((char*))&err, &len);
        if(!err) err = errno;
      )

      switch (err) {
        case 0: break;
          // Keep alive timeout
        case ETIMEDOUT:
        case ECONNRESET:
        case EPIPE:
          disconnect();
          [[fallthrough]];
          // No data
        case EAGAIN: return DataBuffer();
        default:
          disconnect();
        return DataBuffer();
      }
    }

    if(!size) return DataBuffer();
    buffer.resize(size);
    recv(client_socket, (char*)buffer.data(), buffer.size(), 0);
    return buffer;
}

DataBuffer TcpClient::loadDataSync() {
  DataBuffer data;
  uint32_t size = 0;
  int answ = recv(client_socket, reinterpret_cast<char*>(&size), sizeof(size), 0);
  if(size && answ == sizeof (size)) {
    data.resize(size);
    recv(client_socket, reinterpret_cast<char*>(data.data()), data.size(), 0);
  }
  return data;
}

void TcpClient::setHandler(TcpClient::handler_function_t handler) {

  {
    std::lock_guard lock(handle_mutex);
    handler_func = handler;
  }

  switch (thread_management_type) {
    case stcp::TcpClient::ThreadManagementType::single_thread:
      if(threads.thread) return;
      threads.thread = new std::thread(&TcpClient::handleSingleThread, this);
    break;
    case stcp::TcpClient::ThreadManagementType::thread_pool:
      threads.thread_pool->addJob([this]{handleThreadPool();});
    break;
  }
}

void TcpClient::joinHandler() {
  switch (thread_management_type) {
    case stcp::TcpClient::ThreadManagementType::single_thread:
      if(threads.thread) threads.thread->join();
    break;
    case stcp::TcpClient::ThreadManagementType::thread_pool:
      threads.thread_pool->join();
    break;
  }
}

bool TcpClient::sendData(const void* buffer, const size_t size) const {
  void* send_buffer = malloc(size + sizeof (int));
  memcpy(reinterpret_cast<char*>(send_buffer) + sizeof(int), buffer, size);
  *reinterpret_cast<int*>(send_buffer) = size;
  if(send(client_socket, reinterpret_cast<char*>(send_buffer), size + sizeof(int), 0) < 0) return false;
  free(send_buffer);
	return true;
}

uint32_t TcpClient::getHost() const {return NIX(address.sin_addr.s_addr) WIN(address.sin_addr.S_un.S_addr);}
uint16_t TcpClient::getPort() const {return address.sin_port;}
