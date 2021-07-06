# TcpServer
Simple Crossplatform Multi-threading TCP/IP Server

**Build**

```gcc``` Required

```bash
make all
```

**Usage example:**
```cpp
#include "server/hdr/TcpServer.h"

#include <iostream>

//Parse ip to std::string
std::string getHostStr(const TcpServer::Client& client) {
uint32_t ip = client.getHost ();
return std::string() + std::to_string(int(reinterpret_cast<char*>(&ip)[0])) + '.' +
        std::to_string(int(reinterpret_cast<char*>(&ip)[1])) + '.' +
        std::to_string(int(reinterpret_cast<char*>(&ip)[2])) + '.' +
        std::to_string(int(reinterpret_cast<char*>(&ip)[3])) + ':' +
        std::to_string( client.getPort ());
}

int main() {
    //Create object of TcpServer
    TcpServer server( 8080, [](DataBuffer data, TcpServer::Client& client){
        std::cout << "("<<getHostStr(client)<<")[ " << data.size << " bytes ]: " << (char*)data.data_ptr << '\n';
        client.sendData("Hello, client!", sizeof("Hello, client!"));
        }, {1, 1, 1}); // Keep alive{idle:1s, interval: 1s, package count: 1};
    
    //Start server
    if(server.start() == TcpServer::status::up) {
        std::cout<<"Server is up!"<<std::endl;
        server.joinLoop(); //Joing to the client handling loop
    } else {
        std::cout<<"Server start error! Error code:"<< int(server.getStatus()) <<std::endl;
        return -1;
    }

}
```

**New client handling way**

<img src="https://raw.githubusercontent.com/gbytegear/TcpServer/master/doc/ClientHandling.jpg">
