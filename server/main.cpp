#include "tcp/include/TcpServer.h"

#include <iostream>

using namespace stcp;

//Parse ip to std::string
std::string getHostStr(const TcpServer::Client& client) {
    uint32_t ip = client.getHost ();
    return std::string() + std::to_string(int(reinterpret_cast<char*>(&ip)[0])) + '.' +
            std::to_string(int(reinterpret_cast<char*>(&ip)[1])) + '.' +
            std::to_string(int(reinterpret_cast<char*>(&ip)[2])) + '.' +
            std::to_string(int(reinterpret_cast<char*>(&ip)[3])) + ':' +
            std::to_string( client.getPort ());
}

TcpServer server(8081,
{1, 1, 1}, // Keep alive{idle:1s, interval: 1s, pk_count: 1}

[](DataBuffer data, TcpServer::Client& client){ // Data handler
  std::cout << "Client "<< getHostStr(client) <<" send data [ " << data.size() << " bytes ]: " << (char*)data.data() << '\n';
  client.sendData("Hello, client!\0", sizeof("Hello, client!\0"));
},

[](TcpServer::Client& client) { // Connect handler
  std::cout << "Client " << getHostStr(client) << " connected\n";
},

[](TcpServer::Client& client) { // Disconnect handler
  std::cout << "Client " << getHostStr(client) << " disconnected\n";
},

std::thread::hardware_concurrency() // Thread pool size
);



int main() {
  using namespace std::chrono_literals;
  try {
    //Start server
    if(server.start() == TcpServer::status::up) {
      std::cout<<"Server listen on port: " << server.getPort() << std::endl
               <<"Server handling thread pool size: " << server.getThreadPool().getThreadCount() << std::endl;
      server.joinLoop();
      return EXIT_SUCCESS;
    } else {
      std::cout<<"Server start error! Error code:"<< int(server.getStatus()) <<std::endl;
      return EXIT_FAILURE;
    }

  } catch(std::exception& except) {
    std::cerr << except.what();
    return EXIT_FAILURE;
  }
}
