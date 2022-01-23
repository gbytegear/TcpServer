#include "tcp/include/TcpClient.h"

#include <iostream>
#include <stdlib.h>
#include <thread>

using namespace stcp;

std::string getHostStr(uint32_t ip, uint16_t port) {
    return std::string() + std::to_string(int(reinterpret_cast<char*>(&ip)[0])) + '.' +
            std::to_string(int(reinterpret_cast<char*>(&ip)[1])) + '.' +
            std::to_string(int(reinterpret_cast<char*>(&ip)[2])) + '.' +
            std::to_string(int(reinterpret_cast<char*>(&ip)[3])) + ':' +
            std::to_string( port );
}

int main() {
  using namespace std::chrono_literals;
  TcpClient client;
  if(client.connectTo(LOCALHOST_IP, 8081) == SocketStatus::connected) {
      std::clog << "Client connected\n";

        client.setHandler([&client](DataBuffer data) {
          std::clog << "Recived " << data.size() << " bytes: " << (char*)data.data() << '\n';
          std::this_thread::sleep_for(1s);
          client.sendData("Hello, server\0", sizeof ("Hello, server\0"));
        });
        client.sendData("Hello, server\0", sizeof ("Hello, server\0"));
        client.joinHandler();
  } else {
    std::cerr << "Client isn't connected\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
