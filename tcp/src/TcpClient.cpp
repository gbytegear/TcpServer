#include "../hdr/TcpClient.h"
#include <stdio.h>

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
}

TcpClient::status TcpClient::connectTo(uint32_t host, uint16_t port) noexcept {
  WIN(if(WSAStartup(MAKEWORD(2, 2), &w_data) != 0) {})

  if((client_socket = socket (AF_INET, SOCK_STREAM, IPPROTO_IP))
     WIN(== SOCKET_ERROR)
     NIX(< 0)
     ) return _status = status::err_socket_init;

	sockaddr_in dest_addr;
	dest_addr.sin_family = AF_INET;
  dest_addr.sin_addr.s_addr = host;
  WINIX(
      dest_addr.sin_addr.S_un.S_addr = host;
      ,
      dest_addr.sin_addr.s_addr = host;
  )
	dest_addr.sin_port = htons(port);

  if(connect(client_socket, (sockaddr *)&dest_addr, sizeof(dest_addr))
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
  WINIX(
    closesocket(client_socket);
      ,
    close(client_socket);
  )
	return _status = status::disconnected;
}

int TcpClient::loadData() {return recv (client_socket, buffer, buffer_size, 0);}
char* TcpClient::getData() {return buffer;}

bool TcpClient::sendData(const char* buffer, const size_t size) const {
	if(send(client_socket, buffer, size, 0) < 0) return false;
	return true;
}
