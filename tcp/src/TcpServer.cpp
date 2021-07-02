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

TcpServer::TcpServer(const uint16_t port, handler_function_t handler) : port(port), handler(handler) {}

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

DataBuffer TcpServer::Client::loadData() {
  DataBuffer buffer;

  // Read data length
  // MSG_DONTWAIT - Unix non-blocking read
  WIN(if(u_long t = true; SOCKET_ERROR == ioctlsocket(socket, FIONBIO, &t)) return DataBuffer();) // Windows non-blocking mode on
  WIN(int readed_size =) recv(socket, (char*)&buffer.size, sizeof (buffer.size), NIX(MSG_DONTWAIT)WIN(0));
  WIN(if(u_long t = false; SOCKET_ERROR == ioctlsocket(socket, FIONBIO, &t)) return DataBuffer();) // Windows non-blocking mode off

  // Keep alive timeout
  if(errno == ETIMEDOUT ||
     errno == ECONNRESET) {
    _status = status::disconnected;
    return DataBuffer();
  }

  // Read data
  if(NIX(errno != EAGAIN)WIN(readed_size != SOCKET_ERROR)) {
    if(!buffer.size) return DataBuffer();
    buffer.data_ptr = (char*)malloc(buffer.size);
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
  int flag;
  if(_status == status::up) stop();

  WIN(if(WSAStartup(MAKEWORD(2, 2), &w_data) == 0) {})
  SocketAddr_in address;
  address.sin_addr
      WIN(.S_un.S_addr)NIX(.s_addr) = INADDR_ANY;
  address.sin_port = htons(port);
  address.sin_family = AF_INET;


  if((serv_socket = socket(AF_INET, SOCK_STREAM, 0)) WIN(== SOCKET_ERROR)NIX(== -1))
     return _status = status::err_socket_init;

  flag = true;
  if((setsockopt(serv_socket, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) == -1) ||
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

      client_mutex.lock();
      client_list.emplace_back(Client(client_socket, client_addr));
      client_mutex.unlock();
      if(client_handler_threads.empty())
        client_handler_threads.emplace_back(std::thread([this]{clientHandler(client_list.begin());}));
    }
  }

}

bool TcpServer::enableKeepAlive(Socket socket) {
NIX(
  int flag = 1;
  if(setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag)) == -1) return false;
  if(setsockopt(socket, IPPROTO_TCP, TCP_KEEPIDLE, &ka_conf.ka_idle, sizeof(ka_conf.ka_idle)) == -1) return false;
  if(setsockopt(socket, IPPROTO_TCP, TCP_KEEPINTVL, &ka_conf.ka_intvl, sizeof(ka_conf.ka_intvl)) == -1) return false;
  if(setsockopt(socket, IPPROTO_TCP, TCP_KEEPCNT, &ka_conf.ka_cnt, sizeof(ka_conf.ka_cnt)) == -1) return false;
  return true;
)WIN(
  return false; // TODO: Windows support
)
}


void TcpServer::clientHandler(std::list<Client>::iterator cur_client) {
  std::thread::id this_thr_id = std::this_thread::get_id();

  auto moveNextClient = [&cur_client, this]()->bool{
    client_mutex.lock_shared();
    if(++cur_client == client_list.end()) {
      cur_client = client_list.begin();
      if(cur_client == client_list.end()) { // if list is empty
        client_mutex.unlock_shared();
        return false;
      }
    }
    client_mutex.unlock_shared();
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


  while(_status == status::up) {
    // If client isn't handled now
    if(!cur_client->access_mtx.try_lock()) { // <--- Segmentation fault
      if(moveNextClient()) continue;         // (Element has been deleted from other thread)
      else { removeThread(); return; }
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
      client_list.splice(client_list.cend(), client_list, cur_client);
      cur_client->access_mtx.unlock();
      client_mutex.unlock();

    } else {
      cur_client->access_mtx.unlock();
      if(moveNextClient()) continue;
      else {removeThread(); return;}
    }

    removeThread();
    return;

  }


}

TcpServer::Client::Client(Socket socket, SocketAddr_in address)
  : address(address), socket(socket) {}
TcpServer::Client::Client(Client&& other)
  : access_mtx(), address(other.address), socket(other.socket) {
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

