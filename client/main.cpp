#include "tcp/hdr/TcpClient.h"

#include <iostream>
#include <stdlib.h>
#include <thread>

using namespace std;

std::string getHostStr(uint32_t ip, uint16_t port) {
    return std::string() + std::to_string(int(reinterpret_cast<char*>(&ip)[0])) + '.' +
            std::to_string(int(reinterpret_cast<char*>(&ip)[1])) + '.' +
            std::to_string(int(reinterpret_cast<char*>(&ip)[2])) + '.' +
            std::to_string(int(reinterpret_cast<char*>(&ip)[3])) + ':' +
            std::to_string( port );
}

int main() {
  static int call_count = 5;
  using namespace std::chrono_literals;
  TcpClient client;
  if(client.connectTo(LOCALHOST_IP, 8080) == SocketStatus::connected) {
    std::clog << "Client connected\n";
  } else {
    std::cerr << "Client isn't connected\n";
    return -1;
  }
  for(int i = 0; i < 255; ++i) {
    std::clog << "Client tries to send data to server\n";
    client.sendData("Hello, server!", sizeof("Hello, server!"));
    std::clog << "Client waites data from server...\n";
    DataBuffer data = client.loadData();
    std::cout << "Client[ " << data.size << " bytes ]: " << (const char*)data.data_ptr << '\n';
  }
  client.disconnect();
  std::clog << "Socket closed!\n";
  if(--call_count) return main();
  return 0;
}
