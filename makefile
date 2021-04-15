libtcp.a:
	g++ -c -I. tcp/src/*.cpp -lpthread
	ar cr libtcp.a ./*.o
	rm -rf ./*.o

client: libtcp.a
	g++ ./client/main.cpp -I. -L/home/oldev/projects/TcpServer -o chat_client -lpthread -ltcp


server: libtcp.a
	g++ ./server/main.cpp -I. -L/home/oldev/projects/TcpServer -o chat_server -lpthread -ltcp

lib: libtcp.a

all: client server

clean:
	rm -rf ./chat_* ./libtcp.a ./TcpServer.pro.* ./TcpClient.pro.*
