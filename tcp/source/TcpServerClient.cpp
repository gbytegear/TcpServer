#include "../include/TcpServer.h"

using namespace stcp;

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



#include <iostream>

DataBuffer TcpServer::Client::loadData() {
  if(_status != SocketStatus::connected) return DataBuffer();
  using namespace std::chrono_literals;
  DataBuffer buffer;
  uint32_t size;
  int err;

  // Read data length in non-blocking mode
  // MSG_DONTWAIT - Unix non-blocking read
  WIN(if(u_long t = true; SOCKET_ERROR == ioctlsocket(socket, FIONBIO, &t)) return DataBuffer();) // Windows non-blocking mode on
  int answ = recv(socket, (char*)&size, sizeof(size), NIX(MSG_DONTWAIT)WIN(0));

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
        getsockopt (socket, SOL_SOCKET, SO_ERROR, WIN((char*))&err, &len);
      }
    )NIX(
      SockLen_t len = sizeof (err);
      getsockopt (socket, SOL_SOCKET, SO_ERROR, WIN((char*))&err, &len);
      if(!err) err = errno;
    )

    WIN(if(u_long t = false; SOCKET_ERROR == ioctlsocket(socket, FIONBIO, &t)) return DataBuffer();) // Windows non-blocking mode off

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
        std::cerr << "Unhandled error!\n"
                    << "Code: " << err << " Err: " << std::strerror(err) << '\n';
      return DataBuffer();
    }
  }

  if(!size) return DataBuffer();
  buffer.resize(size);
  recv(socket, buffer.data(), buffer.size(), 0);
  return buffer;
}


TcpClientBase::status TcpServer::Client::disconnect() {
  _status = status::disconnected;
  if(socket == WIN(INVALID_SOCKET)NIX(-1)) return _status;
  shutdown(socket, SD_BOTH);
  WIN(closesocket)NIX(close)(socket);
  socket = WIN(INVALID_SOCKET)NIX(-1);
  return _status;
}

bool TcpServer::Client::sendData(const void* buffer, const size_t size) const {
  if(_status != SocketStatus::connected) return false;

  void* send_buffer = malloc(size + sizeof (uint32_t));
  memcpy(reinterpret_cast<char*>(send_buffer) + sizeof(uint32_t), buffer, size);
  *reinterpret_cast<uint32_t*>(send_buffer) = size;

  if(send(socket, reinterpret_cast<char*>(send_buffer), size + sizeof (int), 0) < 0) return false;

  free(send_buffer);
  return true;
}

TcpServer::Client::Client(Socket socket, SocketAddr_in address)
  : address(address), socket(socket) {}


TcpServer::Client::~Client() {
  if(socket == WIN(INVALID_SOCKET)NIX(-1)) return;
  shutdown(socket, SD_BOTH);
  WIN(closesocket(socket);)
  NIX(close(socket);)
}

uint32_t TcpServer::Client::getHost() const {return NIX(address.sin_addr.s_addr) WIN(address.sin_addr.S_un.S_addr);}
uint16_t TcpServer::Client::getPort() const {return address.sin_port;}

