#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#define localhost 0x0100007f

#include <cstdint>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET socket_t;
#else
typedef int socket_t;
#endif

#define buffer_size 4096

struct TcpClient {

	enum class status : uint8_t {
		connected = 0,
		err_socket_init = 1,
		err_socket_bind = 2,
		err_socket_connect = 3,
		disconnected = 1
	};

private:
	char buffer[buffer_size];
	status _status = status::disconnected;
	socket_t client_socket;
#ifdef _WIN32
	WSAData w_data;
#else
#endif
public:
	TcpClient() noexcept;
	~TcpClient();

	status connectTo(uint32_t host, uint16_t port) noexcept;
	status disconnect() noexcept;

	uint32_t getHost() const;
	uint16_t getPort() const;

	status getStatus() {return _status;}

	int loadData();
	char* getData();

	bool sendData(const char* buffer, const size_t size) const;
};

#endif // TCPCLIENT_H
