
ifeq ($(UNAME), Linux)
CXX_LIBS = -lpthread
else
CXX_LIBS = -lpthread -lws2_32
endif



libtcp.a:
	g++ -c -I. tcp/source/*.cpp -std=c++17
	ar cr libtcp.a ./*.o
	rm -rf ./*.o

client: libtcp.a
	g++ ./client/main.cpp -I. -L/home/oldev/projects/TcpServer -o test_client -L./ -ltcp $(CXX_LIBS) -std=c++17


server: libtcp.a
	g++ ./server/main.cpp -I. -L/home/oldev/projects/TcpServer -o test_server -L./ -ltcp $(CXX_LIBS) -std=c++17

lib: libtcp.a

all: client server

clean:
	rm -rf ./test_* ./libtcp.a ./TcpServer.pro.* ./TcpClient.pro.* *.o
