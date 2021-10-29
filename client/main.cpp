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
  if(client.connectTo(LOCALHOST_IP, 33333) == SocketStatus::connected) {

    client.setHandler([](DataBuffer data) {
                        std::clog << "Recived " << data.size << " bytes\n";
                      });


    std::clog << "Client connected\n";
  } else {
    std::cerr << "Client isn't connected\n";
    return -1;
  }
  if(--call_count) return main();
  return 0;
}
