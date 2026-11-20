#include "blank_partition.h"
#include <assert.h>
#include <string.h>
#include <vector>
#include <iostream>
int main() {
  std::vector<uint8_t> data(1031,0xff);uint8_t buffer[128];size_t reads=0,yields=0;
  auto inspect=[&](size_t failAt=size_t(-1)) {
    reads=yields=0;
    return dragon::inspectBlankRegion(data.size(),buffer,sizeof(buffer),
      [&](size_t offset,uint8_t* out,size_t count){++reads;if(offset==failAt)return false;assert(offset+count<=data.size());memcpy(out,data.data()+offset,count);return true;},
      [&](){++yields;});
  };
  assert(inspect()==dragon::BlankRegion::Blank);assert(reads==9&&yields==9);
  // Any single programmed byte, including final partial chunk, prevents format.
  for(size_t i=0;i<data.size();++i){data[i]=0x7f;assert(inspect()==dragon::BlankRegion::ExistingData);data[i]=0xff;}
  for(size_t offset=0;offset<data.size();offset+=sizeof(buffer))assert(inspect(offset)==dragon::BlankRegion::ReadFailure);
  assert(dragon::inspectBlankRegion(0,buffer,sizeof(buffer),[](size_t,uint8_t*,size_t){assert(false);return true;},[](){})==dragon::BlankRegion::InvalidArguments);
  assert(dragon::inspectBlankRegion(1,nullptr,1,[](size_t,uint8_t*,size_t){assert(false);return true;},[](){})==dragon::BlankRegion::InvalidArguments);
  std::cout<<"Blank partition policy PASS: every programmed byte/read failure blocks initialization, partial tail included\n";
}
