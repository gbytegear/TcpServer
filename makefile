libtcp.a:
	g++ -c -I. tcp/src/*.cpp -lpthread -std=c++17
	ar cr libtcp.a ./*.o
	rm -rf ./*.o

client: libtcp.a
	g++ ./client/main.cpp -I. -L/home/oldev/projects/TcpServer -o test_client -ltcp -lpthread -std=c++17


server: libtcp.a
	g++ ./server/main.cpp -I. -L/home/oldev/projects/TcpServer -o test_server -ltcp -lpthread -std=c++17

lib: libtcp.a

all: client server

clean:
	rm -rf ./test_* ./libtcp.a ./TcpServer.pro.* ./TcpClient.pro.* *.o
