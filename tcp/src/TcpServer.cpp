#include "../hdr/TcpServer.h"
#include <chrono>

TcpServer::TcpServer(const uint16_t port, handler_function_t handler) : port(port), handler(handler) {}

TcpServer::~TcpServer() {
  if(_status == status::up)
	stop();
#ifdef _WIN32 // Windows NT
	WSACleanup ();
#endif
}


void TcpServer::setHandler(TcpServer::handler_function_t handler) {this->handler = handler;}

uint16_t TcpServer::getPort() const {return port;}
uint16_t TcpServer::setPort( const uint16_t port) {
	this->port = port;
	restart();
	return port;
}

TcpServer::status TcpServer::restart() {
	if(_status == status::up)
	  stop ();
	return start();
}

int TcpServer::Client::loadData() {return recv(socket, buffer, buffer_size, 0);}
char* TcpServer::Client::getData() {return buffer;}

bool TcpServer::Client::sendData(const char* buffer, const size_t size) const {
  if(send(socket, buffer, size, 0) < 0) return false;
  return true;
}


#ifdef _WIN32 // Windows NT
TcpServer::status TcpServer::start() {
	if(WSAStartup(MAKEWORD(2, 2), &w_data) == 0) {}

	SOCKADDR_IN address;
	address.sin_addr.S_un.S_addr = INADDR_ANY;
	address.sin_port = htons(port);
	address.sin_family = AF_INET;

	if(static_cast<int>(serv_socket = socket(AF_INET, SOCK_STREAM, 0)) == SOCKET_ERROR) return _status = status::err_socket_init;
	if(bind(serv_socket, (struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) return _status = status::err_socket_bind;
	if(listen(serv_socket, SOMAXCONN) == SOCKET_ERROR) return _status = status::err_socket_listening;

	_status = status::up;
	handler_thread = std::thread([this]{handlingLoop();});
	return _status;
}

void TcpServer::stop() {
	_status = status::close;
	closesocket (serv_socket);
	joinLoop();
	for(std::thread& cl_thr : client_handler_threads)
		cl_thr.join();
	client_handler_threads.clear ();
	client_handling_end.clear ();
}

void TcpServer::joinLoop() {handler_thread.join();}

void TcpServer::handlingLoop() {
	while(_status == status::up) {
		SOCKET client_socket;
		SOCKADDR_IN client_addr;
		int addrlen = sizeof(client_addr);
		if ((client_socket = accept(serv_socket, (struct sockaddr*)&client_addr, &addrlen)) != 0 && _status == status::up){
			client_handler_threads.push_back(std::thread([this, &client_socket, &client_addr] {
				handler(Client(client_socket, client_addr));
				client_handling_end.push_back (std::this_thread::get_id());
			}));
		}

		if(!client_handling_end.empty())
		  for(std::list<std::thread::id>::iterator id_it = client_handling_end.begin (); !client_handling_end.empty() ; id_it = client_handling_end.begin())
			for(std::list<std::thread>::iterator thr_it = client_handler_threads.begin (); thr_it != client_handler_threads.end () ; ++thr_it)
			  if(thr_it->get_id () == *id_it) {
				thr_it->join();
				client_handler_threads.erase(thr_it);
				client_handling_end.erase (id_it);
				break;
			  }

		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
}

TcpServer::Client::Client(SOCKET socket, SOCKADDR_IN address) : socket(socket), address(address) {}
TcpServer::Client::Client(const TcpServer::Client& other) : socket(other.socket), address(other.address) {}

TcpServer::Client::~Client() {
	shutdown(socket, 0);
	closesocket(socket);
}

uint32_t TcpServer::Client::getHost() const {return address.sin_addr.S_un.S_addr;}
uint16_t TcpServer::Client::getPort() const {return address.sin_port;}

void TcpServer::Client::connectTo(TcpServer::Client& other_client) const {
	other_client.connected_to = const_cast<Client*>(this);
	while(true) {
		int size = other_client.loadData();
		if(size == 0) return;
		sendData(other_client.getData (), size);
	}
}

void TcpServer::Client::waitConnect() const {
	while (connected_to == nullptr);
	while(true) {
		int size = connected_to->loadData();
		if(size == 0) return;
		sendData(connected_to->getData (), size);
	}
}


#else // *nix

TcpServer::status TcpServer::start() {
	struct sockaddr_in server;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons( port );
	server.sin_family = AF_INET;
	serv_socket = socket(AF_INET, SOCK_STREAM, 0);

	if(serv_socket == -1) return _status = status::err_socket_init;
	if(bind(serv_socket,(struct sockaddr *)&server , sizeof(server)) < 0) return _status = status::err_socket_bind;
	if(listen(serv_socket, 3) < 0)return _status = status::err_socket_listening;

	_status = status::up;
	handler_thread = std::thread([this]{handlingLoop();});
	return _status;
}

void TcpServer::stop() {
	_status = status::close;
	close(serv_socket);
	joinLoop();
	for(std::thread& cl_thr : client_handler_threads)
		cl_thr.join();
	client_handler_threads.clear ();
	client_handling_end.clear ();
}

void TcpServer::joinLoop() {handler_thread.join();}

void TcpServer::handlingLoop() {
	while (_status == status::up) {
		int client_socket;
		struct sockaddr_in client_addr;
		int addrlen = sizeof (struct sockaddr_in);
		if((client_socket = accept(serv_socket, (struct sockaddr*)&client_addr, (socklen_t*)&addrlen)) >= 0 && _status == status::up)
			client_handler_threads.push_back(std::thread([this, &client_socket, &client_addr] {
				handler(Client(client_socket, client_addr));
				client_handling_end.push_back (std::this_thread::get_id());
			}));

		if(!client_handling_end.empty())
		  for(std::list<std::thread::id>::iterator id_it = client_handling_end.begin (); !client_handling_end.empty() ; id_it = client_handling_end.begin())
			for(std::list<std::thread>::iterator thr_it = client_handler_threads.begin (); thr_it != client_handler_threads.end () ; ++thr_it)
			  if(thr_it->get_id () == *id_it) {
				thr_it->join();
				client_handler_threads.erase(thr_it);
				client_handling_end.erase (id_it);
				break;
			  }

		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
}

TcpServer::Client::Client(int socket, struct sockaddr_in address) : socket(socket), address(address) {}
TcpServer::Client::Client(const TcpServer::Client& other) : socket(other.socket), address(other.address) {}

TcpServer::Client::~Client() {
	shutdown(socket, 0);
	close(socket);
}

uint32_t TcpServer::Client::getHost() {return address.sin_addr.s_addr;}
uint16_t TcpServer::Client::getPort() {return address.sin_port;}

#endif
