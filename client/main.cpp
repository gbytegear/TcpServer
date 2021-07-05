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
  using namespace std::chrono_literals;
  TcpClient client;
  TcpClient client_2;
  client.connectTo(LOCALHOST_IP, 8080);
  client_2.connectTo(LOCALHOST_IP, 8080);
  client.sendData("Hello, server!", sizeof("Hello, server!"));
  DataBuffer data = client.loadData();
  std::cout << "Client[ " << data.size << " bytes ]: " << (const char*)data.data_ptr << '\n';
  data.~DataBuffer();
  new(&data) DataBuffer(client_2.loadData());
  std::cout << "Client 2[ " << data.size << " bytes ]: " << (const char*)data.data_ptr << '\n';
  std::this_thread::sleep_for(5s);
  client.disconnect();
  client_2.disconnect();
  std::clog << "Socket closed!\n";
}
