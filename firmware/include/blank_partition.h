#pragma once
#include <stddef.h>
#include <stdint.h>
namespace dragon {
enum class BlankRegion { Blank, ExistingData, ReadFailure, InvalidArguments };
// Reader returns true only for a complete read. Never inspect uninitialized
// bytes after a failed read, and inspect every byte before allowing first-use
// initialization. Independent of ESP-IDF so boundary/error cases are testable.
template<class Reader,class Yield>
BlankRegion inspectBlankRegion(size_t length,uint8_t* buffer,size_t capacity,Reader read,Yield yield) {
  if(!length||!buffer||!capacity)return BlankRegion::InvalidArguments;
  for(size_t offset=0;offset<length;) {
    const size_t count=(length-offset<capacity)?length-offset:capacity;
    if(!read(offset,buffer,count))return BlankRegion::ReadFailure;
    for(size_t i=0;i<count;++i)if(buffer[i]!=0xff)return BlankRegion::ExistingData;
    offset+=count;yield();
  }
  return BlankRegion::Blank;
}
}
