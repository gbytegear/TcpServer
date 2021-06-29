#include "../hdr/TcpServer.h"
#include <chrono>
#include <cstring>
#include <mutex>

#ifdef _WIN32
#define WIN(exp) exp
#define NIX(exp)
#else
#define WIN(exp)
#define NIX(exp) exp
#endif

TcpServer::TcpServer(const uint16_t port, handler_function_t handler, double keep_alive_timeout) : port(port), keep_alive_timeout(keep_alive_timeout), handler(handler) {}

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

DataBuffer TcpServer::Client::loadData() {
  DataBuffer buffer;

  // Read data length
  // MSG_DONTWAIT - Unix non-blocking read
  WIN(if(u_long t = true; SOCKET_ERROR == ioctlsocket(socket, FIONBIO, &t)) return buffer;) // Windows non-blocking mode on
  WIN(int readed_size = )recv(socket, (char*)&buffer.size, sizeof (buffer.size), NIX(MSG_DONTWAIT)WIN(0));

  // Read data
  if(NIX(errno != EAGAIN)WIN(readed_size != SOCKET_ERROR)) {
    keep_alive_counter = time(nullptr);
    if(!buffer.size) return DataBuffer();
    buffer.data_ptr = (char*)malloc(buffer.size);
    WIN(if(u_long t = false; SOCKET_ERROR == ioctlsocket(socket, FIONBIO, &t)) return buffer;) // Windows non-blocking mode off
    recv(socket, (char*)buffer.data_ptr, buffer.size, 0);
    return buffer;
  } WIN(else if(int err = WSAGetLastError(); err != WSAEWOULDBLOCK){
    // TODO: Critical error handle
  })

  return DataBuffer();
}

bool TcpServer::Client::sendData(const char* buffer, const size_t size) const {
  void* send_buffer = malloc(size + sizeof (int));
  memcpy(reinterpret_cast<char*>(send_buffer) + sizeof(int), buffer, size);
  *reinterpret_cast<int*>(send_buffer) = size;
  if(send(socket, reinterpret_cast<char*>(send_buffer), size + sizeof (int), 0) < 0) return false;
  free(send_buffer);
  return true;
}


TcpServer::status TcpServer::start() {
  WIN(
        if(WSAStartup(MAKEWORD(2, 2), &w_data) == 0) {}
        SOCKADDR_IN address;
        address.sin_addr.S_un.S_addr = INADDR_ANY;
        address.sin_port = htons(port);
        address.sin_family = AF_INET;

        if(static_cast<int>(serv_socket = socket(AF_INET, SOCK_STREAM, 0)) == SOCKET_ERROR) return _status = status::err_socket_init;
        if(bind(serv_socket, (struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) return _status = status::err_socket_bind;
        if(listen(serv_socket, SOMAXCONN) == SOCKET_ERROR) return _status = status::err_socket_listening;
  )

  NIX(
        struct sockaddr_in server;
        server.sin_addr.s_addr = INADDR_ANY;
        server.sin_port = htons( port );
        server.sin_family = AF_INET;
        serv_socket = socket(AF_INET, SOCK_STREAM, 0);

        if(serv_socket == -1) return _status = status::err_socket_init;
        if(bind(serv_socket,(struct sockaddr *)&server , sizeof(server)) < 0) return _status = status::err_socket_bind;
        if(listen(serv_socket, 3) < 0)return _status = status::err_socket_listening;
  )

  _status = status::up;
  handler_thread = std::thread([this]{handlingLoop();});
  return _status;
}

void TcpServer::stop() {
  _status = status::close;
  WIN(closesocket(serv_socket);)
  NIX(close(serv_socket);)
  joinLoop();
  client_handler_mutex.lock();
  for(std::thread& cl_thr : client_handler_threads)
    cl_thr.join();
  client_handler_mutex.unlock();
  client_handler_threads.clear();
  client_list.clear();
}

void TcpServer::joinLoop() {handler_thread.join();}

void TcpServer::handlingLoop() {
  NIX(const socklen_t)WIN(int) addrlen = sizeof(SocketAddr_in);
  while (_status == status::up) {
    SocketAddr_in client_addr;
    if (Socket client_socket = accept(serv_socket, (struct sockaddr*)&client_addr, &addrlen);
        client_socket WIN(!= 0)NIX(>= 0) && _status == status::up) {
      client_mutex.lock();
      client_list.emplace_back(Client(client_socket, client_addr));
      client_mutex.unlock();
      if(client_handler_threads.empty())
        client_handler_threads.emplace_back(std::thread([this]{clientHandler(client_list.begin());}));
    }
  }

}

void TcpServer::clientHandler(std::list<Client>::iterator cur_client) {
  std::thread::id this_thr_id = std::this_thread::get_id();

  while(_status == status::up) {
    // If client isn't handled now
    if(!cur_client->client_mtx.try_lock()) {
      client_mutex.lock_shared();
      if(++cur_client == client_list.end()) cur_client = client_list.begin();
      client_mutex.unlock_shared();
      continue;
    }

    if(DataBuffer data = cur_client->loadData(); data.size) {
      client_handler_mutex.lock();
      client_handler_threads.emplace_back([this, cur_client]()mutable{
        client_mutex.lock_shared();
        std::list<Client>::iterator it = (++cur_client == client_list.end())? client_list.begin() : cur_client;
        client_mutex.unlock_shared();
        clientHandler(it);
      });
      client_handler_mutex.unlock();
      Client client = std::move(*cur_client);

      handler(data, client);

      client_mutex.lock();
      cur_client->client_mtx.unlock();
      client_list.splice(client_list.cend(), client_list, cur_client);
      client_mutex.unlock();

    } else if(std::difftime(std::time(nullptr), cur_client->keep_alive_counter) > keep_alive_timeout) {
      std::list<Client>::iterator last = cur_client;

      client_mutex.lock_shared();
      if(++cur_client == client_list.end()) cur_client = client_list.begin();
      client_mutex.unlock_shared();

      client_mutex.lock();
      last->client_mtx.unlock();
      client_list.erase(last);
      client_mutex.unlock();

      continue;
    }

    client_handler_mutex.lock();
    for(auto it = client_handler_threads.begin(),
        end = client_handler_threads.end();
        it != end ;++it) if(it->get_id() == this_thr_id) {
      it->detach();
      client_handler_threads.erase(it);
      break;
    }
    client_handler_mutex.unlock();

  }


}

TcpServer::Client::Client(Socket socket, SocketAddr_in address)
  : address(address), socket(socket), keep_alive_counter(time(nullptr)) {}
TcpServer::Client::Client(Client&& other)
  : client_mtx(), address(other.address), socket(other.socket), keep_alive_counter(other.keep_alive_counter) {
  other.socket = WIN(SOCKET_ERROR)NIX(-1);
}


TcpServer::Client::~Client() {
  if(socket == WIN(INVALID_SOCKET)NIX(-1)) return;
  shutdown(socket, 0);
  WIN(closesocket(socket);)
  NIX(close(socket);)
}

uint32_t TcpServer::Client::getHost() const {return NIX(address.sin_addr.s_addr) WIN(address.sin_addr.S_un.S_addr);}
uint16_t TcpServer::Client::getPort() const {return address.sin_port;}

