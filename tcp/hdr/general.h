#ifndef GENERAL_H
#define GENERAL_H

#include <cstring>
#include <cinttypes>
#include <chrono>
constexpr uint32_t LOCALHOST_IP = 0x0100007f;

struct DataBuffer {
  int size;
  void* data_ptr;
};

#endif // GENERAL_H
