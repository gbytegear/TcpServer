# TcpServer
Simple Crossplatform Multi-threading TCP/IP Server

# Build library

Required:
 * GCC ([MinGW for Windows](https://sourceforge.net/projects/mingw/))
 * GNU Make ([Make for Windows](http://gnuwin32.sourceforge.net/packages/make.htm))

```bash
# In directory with source code
$ make lib
```

# Usage example:
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

# Build project example:

Copy the compiled library and the folder with the header files ```tcp/hdr``` to the directory with your project and build your project as follows:
```bash
$ g++ <your_code.cpp> -I<path/to/header/files> -L<path/to/static_library> -o <name_of_your_programm> -ltcp -lpthread -std=c++17
```

# Other:
**New client handling way**

<img src="https://raw.githubusercontent.com/gbytegear/TcpServer/master/doc/ClientHandling.jpg">
