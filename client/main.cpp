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


void client() {
  using namespace std::chrono_literals;
  TcpClient client;
  client.connectTo(LOCALHOST_IP, 8080);
  client.sendData("Hello, server!", sizeof("Hello, server!"));
  DataBuffer data = client.loadData();
  std::cout << "Client[ " << data.size << " bytes ]: " << (const char*)data.data_ptr << '\n';
  std::this_thread::sleep_for(5s);
  client.disconnect();
  std::clog << "Socket closed!\n";
}

int main() {
  std::thread th1(client);
  std::thread th2(client);
  std::thread th3(client);
  th1.join();
  th2.join();
  th3.join();
}
