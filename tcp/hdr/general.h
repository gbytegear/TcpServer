#ifndef GENERAL_H
#define GENERAL_H

#include <cstdint>
#include <cstring>
#include <cinttypes>
#include <malloc.h>

constexpr uint32_t LOCALHOST_IP = 0x0100007f;

enum class SocketStatus : uint8_t {
  connected = 0,
  err_socket_init = 1,
  err_socket_bind = 2,
  err_socket_connect = 3,
  disconnected = 1
};

struct DataBuffer {
  int size = 0;
  void* data_ptr = nullptr;

  DataBuffer() = default;
  DataBuffer(int size, void* data_ptr) : size(size), data_ptr(data_ptr) {}
  DataBuffer(const DataBuffer& other) : size(other.size), data_ptr(malloc(size)) {memcpy(data_ptr, other.data_ptr, size);}
  DataBuffer(DataBuffer&& other) : size(other.size), data_ptr(other.data_ptr) {other.data_ptr = nullptr;}
  ~DataBuffer() {if(data_ptr) free(data_ptr);}

  bool isEmpty() {return !data_ptr || !size;}

};

#endif // GENERAL_H
