#include "tcp/include/TcpClient.h"

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
//  static int call_count = 5;
  using namespace std::chrono_literals;
  TcpClient client;
  if(client.connectTo(LOCALHOST_IP, 8081) == SocketStatus::connected) {

    client.setHandler([](DataBuffer data) {
      std::clog << "Recived " << data.size << " bytes: " << (char*)data.data_ptr << '\n';
    });
    client.sendData("Hello, server\0", sizeof ("Hello, server\0"));

    std::clog << "Client connected\n";
  } else {
    std::cerr << "Client isn't connected\n";
    return -1;
  }
//  if(--call_count) return main();
  return 0;
}
