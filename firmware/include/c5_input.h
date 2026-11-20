#pragma once
#include <stddef.h>
#include <stdint.h>

namespace piratebox {
enum class InputMode : uint8_t { Off, Encoder, Joystick, Buttons };
struct InputConfig {
  InputMode mode=InputMode::Off;
  // Encoder: A/B/click. Five switches: up/down/left/right/select.
  uint8_t pins[5]{4,5,6,7,8};
};
inline bool validInputConfig(const InputConfig& config){
  if(config.mode==InputMode::Off)return true;
  const unsigned count=config.mode==InputMode::Encoder?3:
    (config.mode==InputMode::Joystick||config.mode==InputMode::Buttons)?5:0;
  if(!count)return false;
  uint16_t seen=0;
  for(unsigned i=0;i<count;++i){
    // These are CH32 EXIO numbers, not ESP32 GPIO numbers. Reserved board
    // functions and UART/I2C/SPI/USB pins cannot be assigned by this API.
    const uint8_t pin=config.pins[i];
    if(pin<4||pin>14||(seen&(1U<<pin)))return false;
    seen|=1U<<pin;
  }
  return true;
}
class DirectionButton {
  bool raw=false,stable=false,armed=false,repeated=false;
  uint32_t changed=0,pressed=0,lastRepeat=0;
public:
  bool update(bool down,uint32_t now){
    if(down!=raw){raw=down;changed=now;}
    if(!raw&&!stable)armed=true;
    if(raw!=stable&&uint32_t(now-changed)>=25){
      stable=raw;
      if(!stable){armed=true;repeated=false;return false;}
      pressed=lastRepeat=now;repeated=false;
      return armed;
    }
    if(stable&&armed&&uint32_t(now-pressed)>=450&&
       (!repeated||uint32_t(now-lastRepeat)>=170)){
      repeated=true;lastRepeat=now;return true;
    }
    return false;
  }
};
}
