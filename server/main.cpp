#include "tcp/hdr/TcpServer.h"
#include "tcp/src/TcpServer.cpp"

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

int main() {

    TcpServer::Client* wait_client = nullptr;
    std::mutex mtx;

    //Create object of TcpServer
    TcpServer server( 8080,
                      [&wait_client, &mtx](TcpServer::Client client){
        mtx.lock();
        if(wait_client == nullptr){ // Client 2 Client connection
            wait_client = &client;
            std::cout << "Client " << getHostStr(client) << " wait other client...\n";
            mtx.unlock();
            client.waitConnect();
        } else {
            TcpServer::Client* other_client = wait_client;
            wait_client = nullptr;
            std::cout << "Client " << getHostStr(client) << " connected to client " << getHostStr(*other_client) <<'\n';
            mtx.unlock();
            client.connectTo (*other_client);
        }
        std::cout << "Client pair disconnected" << std::endl;
    });

    //Start server
    if(server.start() == TcpServer::status::up) {
        std::cout<<"Server listen on port:"<<server.getPort()<<std::endl;
        server.joinLoop();
    } else {
        std::cout<<"Server start error! Error code:"<< int(server.getStatus()) <<std::endl;
        return -1;
    }

}
