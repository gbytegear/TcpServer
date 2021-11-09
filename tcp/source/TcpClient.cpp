#include "../include/TcpClient.h"
#include <stdio.h>
#include <cstring>

#ifdef _WIN32
#define WIN(exp) exp
#define NIX(exp)
#define WINIX(win_exp, nix_exp) win_exp
#else
#define WIN(exp)
#define NIX(exp) exp
#define WINIX(win_exp, nix_exp) nix_exp
#endif


TcpClient::TcpClient() noexcept : _status(status::disconnected) {}

TcpClient::~TcpClient() {
  disconnect();
  WIN(WSACleanup();)

  if(handler_thread) if(handler_thread->joinable())
      handler_thread->join();
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
  shutdown(client_socket, SD_BOTH);
  WINIX(closesocket(client_socket), close(client_socket));
  _status = status::disconnected;
  if(handler_thread) handler_thread->join();
  return _status;
}

DataBuffer TcpClient::loadData() {
  DataBuffer data;
  recv(client_socket, reinterpret_cast<char*>(&data.size), sizeof(data.size), 0);
  if(data.size) {
    data.data_ptr = malloc(data.size);
    recv(client_socket, reinterpret_cast<char*>(data.data_ptr), data.size, 0);
  }
  return data;
}

std::thread& TcpClient::setHandler(TcpClient::handler_function_t handler) {
  handle_mutex.lock();
  handler_func = handler;
  handle_mutex.unlock();

  if(handler_thread) return *handler_thread;

  handler_thread = std::unique_ptr<std::thread>(new std::thread([this](){
    while(_status == status::connected)
      if(DataBuffer data = loadData(); data) {
        handle_mutex.lock();
        handler_func(std::move(data));
        handle_mutex.unlock();
      }
  }));

  return *handler_thread;
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
