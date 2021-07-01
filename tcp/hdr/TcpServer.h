#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <cstdint>
#include <functional>
#include <thread>
#include <list>
#include <mutex>
#include <shared_mutex>

#ifdef _WIN32 // Windows NT

#include <WinSock2.h>

#else // *nix

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#endif

#include "general.h"

#ifdef _WIN32 // Windows NT
typedef SOCKADDR_IN SocketAddr_in;
typedef SOCKET Socket;
#else // POSIX
typedef struct sockaddr_in SocketAddr_in;
typedef int Socket;
#endif

//#define buffer_size 4096

struct TcpServer {
  class Client;
  class ClientList;
  typedef std::function<void(DataBuffer, Client&)> handler_function_t;
  enum class status : uint8_t {
    up = 0,
    err_socket_init = 1,
    err_socket_bind = 2,
    err_socket_listening = 3,
    close = 4
  };

private:
  Socket serv_socket;
  uint16_t port;
  status _status = status::close;
  std::time_t keep_alive_timeout;
  handler_function_t handler;
  std::thread handler_thread;

  std::list<Client> client_list;
  std::shared_mutex client_mutex;
  std::list<std::thread> client_handler_threads;
  std::mutex client_handler_mutex;

#ifdef _WIN32 // Windows NT
  WSAData w_data;
#endif

  void handlingLoop();
  void clientHandler(std::list<Client>::iterator cur_client);

public:
  TcpServer(const uint16_t port,
            handler_function_t handler,
            std::time_t keep_alive_timeout = 120.);
  ~TcpServer();

  //! Set client handler
  void setHandler(handler_function_t handler);

  uint16_t getPort() const;
  uint16_t setPort(const uint16_t port);

  status getStatus() const {return _status;}

  status restart();
  status start();
  void stop();

  void joinLoop();
};

class TcpServer::Client {
  friend struct TcpServer;

  std::mutex client_mtx;
  SocketAddr_in address;
  Socket socket;
  std::time_t keep_alive_counter;

  DataBuffer loadData();
public:
  Client(Socket socket, SocketAddr_in address);
  Client(Client&& other);
  ~Client();
  uint32_t getHost() const;
  uint16_t getPort() const;

  bool sendData(const char* buffer, const size_t size) const;
};

class TcpServer::ClientList {
  struct Node {
    Node* next = nullptr;
    Node* prev = nullptr;
    Client client;
  };

  Node* first = nullptr;
  Node* last = nullptr;

public:

  class iterator {
    Node* pointer = nullptr;
    iterator(Node* pointer) : pointer(pointer) {}
    friend class TcpServer::ClientList;
  public:
    iterator(const iterator& other) : pointer(other.pointer) {}
    iterator(iterator&& other) : pointer(other.pointer) {}
    TcpServer::Client* operator->() const {return &pointer->client;}
    TcpServer::Client& operator*() const {return pointer->client;}
    iterator& operator++() {pointer = pointer->next; return *this;}
    iterator& operator--() {pointer = pointer->prev; return *this;}
    iterator operator++(int) {iterator last(pointer); pointer = pointer->next; return last;}
    iterator operator--(int) {iterator last(pointer); pointer = pointer->prev; return last;}
    bool operator==(iterator other) {return pointer == other.pointer;}
    bool operator!=(iterator other) {return pointer != other.pointer;}
  };

//  iterator

  bool isEmpty() {return first == nullptr;}
  void extract(iterator it) {
    if(it.pointer->prev) {
      it.pointer->prev->next = it.pointer->next;
      if(!it.pointer->prev->next) last = it.pointer->prev;
    }
    if(it.pointer->next) {
      it.pointer->next->prev = it.pointer->prev;
      if(!it.pointer->next->prev) first = it.pointer->next;
    }
  }
  iterator begin() {return first;}
  iterator end() {return nullptr;}


};

#endif // TCPSERVER_H
