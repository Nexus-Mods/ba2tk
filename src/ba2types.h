#ifndef BA2TYPES_H
#define BA2TYPES_H


#include <fstream>
#include <string>
#include "ba2exception.h"

#ifdef WIN32
#include <Windows.h>

typedef unsigned char BSAUChar;
typedef unsigned short BSAUShort;
typedef unsigned long BSAULong;
typedef UINT64 BSAHash;

#else // WIN32

#include <stdint.h>

typedef uint8_t BSAUChar;
typedef uint16_t BSAUShort;
typedef uint32_t BSAULong;
typedef uint64_t BSAHash;

#endif // WIN32

template <typename T> static T readType(std::fstream &file)
{
  union {
    char buffer[sizeof(T)];
    T value;
  };
  if (!file.read(buffer, sizeof(T))) {
    throw data_invalid_exception("can't read from ba2");
  }
  return value;
}


template <typename T> static void writeType(std::fstream &file, const T &value)
{
  union {
    char buffer[sizeof(T)];
    T valueTemp;
  };
  valueTemp = value;
  
  file.write(buffer, sizeof(T));
}


#endif // BA2TYPES_H

