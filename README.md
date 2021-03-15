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
  TcpServer server( 8080,

	  [](TcpServer::Client client){

		  //Output address of client
		  std::cout<<"Connected host:"<<getHostStr(client)<<std::endl;

		  //Waiting data from client
		  int size = 0;
		  while (size == 0) size = client.loadData ();

		  //Output size of data and data
		  std::cout
			  <<"size: "<<size<<" bytes"<<std::endl
			  << client.getData() << std::endl;

		  //Send data to client
		  const char answer[] = "Hello World from Server";
		  client.sendData(answer, sizeof (answer));
	  }

  );
  
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
