#ifndef GENERAL_H
#define GENERAL_H

#include <cstring>
#include <cinttypes>
constexpr uint32_t LOCALHOST_IP = 0x0100007f;

struct DataDescriptor {
  size_t size;
  void* data_ptr;
};

#endif // GENERAL_H
