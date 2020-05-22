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

std::string msgs;

void printInMsg(std::string msg) {
	std::system ("cls");
	msgs += "<<" + msg + '\n';
	std::cout<<msgs;
}

void printOutMsg(std::string& msg) {
	std::system ("cls");
	msgs += ">>" + msg + '\n';
	std::cout<<msgs;
}

int main() {
	TcpClient client;

	uint32_t host;
	uint16_t port;

	std::string host_str;
	std::cout<<"Enter host:";
	std::cin>>host_str;
	host = inet_addr(host_str.c_str());
	system("cls");
	std::cout<<host_str<<':';
	std::cin>>port;
	system("cls");

	std::cout << "Try connect to: " << host_str <<':'<< to_string (port)<< std::endl;

	if(client.connectTo (host, port) == TcpClient::status::connected) {
		std::cout << "Connected to: " << getHostStr (localhost, 8080) << std::endl;

		std::thread distant_msg([&client](){
			while(true) {
				int size = client.loadData ();
				if(size == 0) {
					cout << "Disconnected";
					std::exit (0);
				}
				printInMsg (client.getData ());
			}
		});

		while(true) {
			std::string message;
			getline(cin, message, '\n');
			client.sendData (message.c_str(), message.length() + 1);
			printOutMsg (message);
		}
	} else {
		std::cout<<"Error! Client isn't connected! Error code: " << int(client.getStatus ()) << std::endl;
	}
}
