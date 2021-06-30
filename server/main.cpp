#include "tcp/hdr/TcpServer.h"
#include "tcp/hdr/TcpClient.h"

#include <iostream>
#include <mutex>

//Parse ip to std::string
std::string getHostStr(const TcpServer::Client& client) {
    uint32_t ip = client.getHost ();
    return std::string() + std::to_string(int(reinterpret_cast<char*>(&ip)[0])) + '.' +
            std::to_string(int(reinterpret_cast<char*>(&ip)[1])) + '.' +
            std::to_string(int(reinterpret_cast<char*>(&ip)[2])) + '.' +
            std::to_string(int(reinterpret_cast<char*>(&ip)[3])) + ':' +
            std::to_string( client.getPort ());
}

TcpServer server( 8080, [](DataBuffer data, TcpServer::Client& client){
  std::cout << "("<<getHostStr(client)<<")[ " << data.size << " bytes ]: " << (char*)data.data_ptr << '\n';
  client.sendData("Hello, client!", sizeof("Hello, client!"));
}, 1);

void testServer() {
  //Start server
  if(server.start() == TcpServer::status::up) {
      std::cout<<"Server listen on port:"<<server.getPort()<<std::endl;
  } else {
      std::cout<<"Server start error! Error code:"<< int(server.getStatus()) <<std::endl;
  }
}

void testClient() {
  TcpClient client;
  client.connectTo(LOCALHOST_IP, 8080);
  client.sendData("Hello, server!", sizeof("Hello, server!"));
  DataBuffer data = client.loadData();
  std::cout << "Client[ " << data.size << " bytes ]: " << (const char*)data.data_ptr << '\n';
}


int main() {
  using namespace std::chrono_literals;
  try {
  testServer();

  std::thread thr1(testClient);
  std::thread thr2(testClient);
  std::thread thr3(testClient);
  std::thread thr4(testClient);


  thr1.join();
  thr2.join();
  thr3.join();
  thr4.join();

  std::this_thread::sleep_for(6s);
  server.stop();
  } catch(std::exception& except) {
    std::cerr << except.what();
  }
}
