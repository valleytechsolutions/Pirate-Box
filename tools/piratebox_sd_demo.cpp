// Export the real built-in BLE Lab demo and validate the exact DDR1 resource pack.
// Host-only utility; does not connect to a board or run BLE operations.
#include "ble_profile.h"
#include "resource_index.h"
#include <fstream>
#include <iterator>
#include <vector>
#include <iostream>
int main(int argc,char** argv) {
  if(argc!=3)return 2;
  std::ifstream input(argv[1],std::ios::binary);
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
  dragon::ResourceIndex index;
  if(!index.load(bytes.data(),bytes.size())){std::cerr<<index.error<<"\n";return 3;}
  dragon::lab::Profile profile;dragon::lab::demo(profile);
  const auto size=dragon::lab::bytes(profile);
  if(!dragon::lab::valid(profile,size))return 4;
  auto* data=reinterpret_cast<const uint8_t*>(&profile);
  const auto crc=dragon::lab::checksum(data,size);
  std::ofstream output(argv[2],std::ios::binary);
  output.write(reinterpret_cast<const char*>(data),size);
  for(unsigned i=0;i<4;++i)output.put(char(crc>>(8*i)));
  output.close();if(!output)return 5;
  std::cout<<"DDR1 valid: "<<index.count()<<" entries, snapshot "<<index.snapshot
           <<"; DDG1 valid: "<<profile.count<<" attributes, "<<size+4<<" bytes\n";
}
