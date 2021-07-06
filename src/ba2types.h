/*
Nexus Mod Manager 2 BA2 handling

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef BA2TYPES_H
#define BA2TYPES_H


#include <fstream>
#include <string>
#include "ba2exception.h"
#include <stdint.h>

typedef uint8_t BSAUChar;
typedef uint16_t BSAUShort;
typedef uint32_t BSAULong;
typedef uint64_t BSAHash;

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

