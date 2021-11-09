TEMPLATE = app
CONFIG += console static
CONFIG -= app_bundle
CONFIG -= qt

linux-g++ | linux-g++-64 | linux-g++-32 {
LIBS +=  -lpthread
CONFIG += c++2a
} else {
CONFIG += c++17
LIBS +=  -lpthread
QMAKE_LFLAGS += -static
LIBS += C:\Qt\Tools\mingw730_64\x86_64-w64-mingw32\lib\libws2_32.a \
     -static C:\Qt\Tools\mingw730_64\x86_64-w64-mingw32\lib\libwinpthread-1.dll \
    -static-libstdc++ -static-libgcc

QMAKE_LFLAGS    = -Wl,-enable-stdcall-fixup -Wl,-enable-auto-import -Wl,-enable-runtime-pseudo-reloc
QMAKE_LFLAGS_EXCEPTIONS_ON = -mthreads
}

SOURCES += client/main.cpp \
        tcp/source/TcpClient.cpp

HEADERS += \
        tcp/include/TcpClient.h \
        tcp/include/general.h
