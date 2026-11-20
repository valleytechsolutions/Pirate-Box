#pragma once
#include <stdint.h>
#include <stddef.h>
namespace dragon::gpsuart {
// Only documented expansion-header pins; never USB, SD or display pins.
#ifdef BOARD_WS_C5_273
constexpr int Pins[]={12};
constexpr unsigned HeaderPins[]={1};
#else
constexpr int Pins[]={18,17,16,44};
constexpr unsigned HeaderPins[]={7,6,8,4};
#endif
constexpr uint32_t Bauds[]={9600,19200,38400,57600,115200,4800};
enum class Preset:uint8_t {GenericNmea,Atgm336h};
struct Config {bool enabled=false;int rx=Pins[0];uint32_t baud=9600;Preset preset=Preset::GenericNmea;};
inline const char* presetName(Preset preset){return preset==Preset::Atgm336h?"ATGM336H":"Generic NMEA";}
// ATGM336H-5N/-5NR32 factory output: NMEA0183, 9600 baud, 8N1.
// This changes only the host receiver settings; it never enables a disabled
// receiver or transmits configuration commands to the module.
inline void selectPreset(Config& c,Preset preset){
  c.preset=preset;
  if(preset==Preset::Atgm336h){c.rx=Pins[0];c.baud=9600;}
}
template<class T,size_t N> int index(const T (&list)[N],T value){for(size_t i=0;i<N;++i)if(list[i]==value)return int(i);return -1;}
inline bool valid(const Config& c){
  return index(Pins,c.rx)>=0&&index(Bauds,c.baud)>=0&&
    (c.preset==Preset::GenericNmea||(c.preset==Preset::Atgm336h&&c.rx==Pins[0]&&c.baud==9600));
}
inline uint32_t encode(const Config& c){return valid(c)?0x47500200u|(uint32_t(c.preset)<<6)|(uint32_t(index(Bauds,c.baud))<<3)|(uint32_t(index(Pins,c.rx))<<1)|unsigned(c.enabled):0;}
inline bool decode(uint32_t value,Config& c){
  const bool legacy=(value&0xffffffc0u)==0x47500100u;
  if(!legacy&&(value&0xffffff80u)!=0x47500200u)return false;
  unsigned pin=(value>>1)&3,baud=(value>>3)&7;
  if(pin>=sizeof(Pins)/sizeof(*Pins)||baud>=sizeof(Bauds)/sizeof(*Bauds))return false;
  Config next{bool(value&1),Pins[pin],Bauds[baud],legacy?Preset::GenericNmea:Preset((value>>6)&1)};
  if(!valid(next))return false;
  c=next;return true;
}
inline unsigned headerPin(int pin){int i=index(Pins,pin);return i<0?0:HeaderPins[i];}
}
