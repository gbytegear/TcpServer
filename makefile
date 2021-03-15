libtcp.a:
	g++ -c -I. tcp/src/*.cpp -lpthread
	ar cr libtcp.a ./*.o
	rm -rf ./*.o

client: libtcp.a
	g++ ./client/main.cpp -I. -L/home/oldev/projects/TcpServer -ltcp -lpthread -o chat_client


server: libtcp.a
	g++ ./server/main.cpp -I. -L/home/oldev/projects/TcpServer -ltcp -lpthread -o chat_server

lib: libtcp.a

all: client server

