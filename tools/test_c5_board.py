"""Native tests for C5 input safety and production PCF85063 RTC functions."""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
work = root / '.work' / 'c5-board-tests'
work.mkdir(parents=True, exist_ok=True)
source = (root / 'firmware/src/board_c5_273.cpp').read_text()
rtc = source[source.index('bool boardReadRtc('):source.index('C5SensorStatus boardC5Sensors(')]
bcd = source[source.index('int fromBcd('):source.index('uint8_t crc8(')]
board_loop = source[source.index('void boardLoop(){'):source.index('void set_brightness(')]
physical_stats = source[source.index('struct PhysicalInputStats {'):source.index('} physicalInput;')+len('} physicalInput;')]
prelude = r'''
#include "wearable.h"
#include "c5_input.h"
#include "rotary_input.h"
#include <cassert>
#include <cstring>
#include <ctime>
#include <cstdarg>
#include <cstdio>
#include <algorithm>
constexpr uint8_t RtcAddress=0x51;
bool rtcAvailable=true;
uint8_t regs[256]{};
int failRead=-1,failWrite=-1;
unsigned readCalls=0,writeCalls=0;uint8_t lastReadReg=255;size_t lastReadCount=0;
bool readRegisters(uint8_t address,uint8_t reg,uint8_t* out,size_t count){
  ++readCalls;lastReadReg=reg;lastReadCount=count;
  assert(address==RtcAddress);if(reg==failRead)return false;memcpy(out,regs+reg,count);return true;
}
bool writeRegisters(uint8_t address,uint8_t reg,const uint8_t* bytes,size_t count){
  ++writeCalls;
  assert(address==RtcAddress);if(reg==failWrite)return false;memcpy(regs+reg,bytes,count);return true;
}
bool writeRegister(uint8_t address,uint8_t reg,uint8_t value){return writeRegisters(address,reg,&value,1);}
uint32_t nowMs=3210;
uint32_t millis(){return nowMs;}
char serialText[512]{};
struct {void printf(const char* format,...){va_list args;va_start(args,format);vsnprintf(serialText,sizeof(serialText),format,args);va_end(args);}} Serial;
constexpr int C5_KEY_L=2,C5_KEY_R=3,BTN_PIN=28,LOW=0;
int pinLevels[29]{};
int digitalRead(int pin){assert(pin==2||pin==3||pin==28);return pinLevels[pin];}
piratebox::DirectionButton leftKey,rightKey;
dragon::RotaryButton bootKey;
struct {bool expanderReady=false;} sensors;
bool sensorsStarted=false;
int deliveredSteps=0;unsigned deliveredClicks=0,deliveredBacks=0;
void inputEvent(int steps,dragon::RotaryPress press){deliveredSteps+=steps;deliveredClicks+=press==dragon::RotaryPress::Click;deliveredBacks+=press==dragon::RotaryPress::Back;}
void sampleExternalInput(uint32_t){}
void sampleWeather(uint32_t){}
void sampleSlowSensors(uint32_t){}
#ifdef _WIN32
tm* gmtime_r(const time_t* value,tm* result){*result=*gmtime(value);return result;}
#endif
'''
tests = r'''
int main(){
  using namespace piratebox;
  InputConfig config;assert(validInputConfig(config));
  config.mode=InputMode::Encoder;assert(validInputConfig(config));
  config.pins[0]=0;assert(!validInputConfig(config)); // Never LCD reset.
  config.pins[0]=3;assert(!validInputConfig(config)); // Never power-control input.
  config.pins[0]=15;assert(!validInputConfig(config));
  config.pins[0]=5;assert(!validInputConfig(config)); // Duplicate.
  config.pins[0]=14;assert(validInputConfig(config));
  config.mode=InputMode::Buttons;assert(validInputConfig(config));
  config.pins[4]=14;assert(!validInputConfig(config));
  config.mode=InputMode(255);assert(!validInputConfig(config));
  DirectionButton key;
  assert(!key.update(false,0));assert(!key.update(true,10));assert(!key.update(false,20));
  assert(!key.update(true,30));assert(key.update(true,55));assert(!key.update(true,56));
  assert(!key.update(true,504));assert(key.update(true,505));assert(!key.update(true,674));
  assert(key.update(true,675));assert(!key.update(false,680));assert(!key.update(false,705));
  DirectionButton held;
  assert(!held.update(true,0));assert(!held.update(true,25));assert(!held.update(true,1000));
  assert(!held.update(false,1010));assert(!held.update(false,1035));
  assert(!held.update(true,1040));assert(held.update(true,1065));
  DirectionButton wrap;wrap.update(false,0xfffffff0u);wrap.update(true,0xfffffff5u);
  assert(wrap.update(true,30)); // Millisecond wrap preserves debouncing.
  dragon::RotaryButton click;
  click.update(false,0);click.update(true,10);click.update(true,50);
  assert(click.update(true,850)==dragon::RotaryPress::Back);
  click.update(false,900);assert(click.update(false,940)==dragon::RotaryPress::None);
  uint32_t epoch=0;
  for(auto date:{dragon::Calendar{2026,10,31,12,0,0},dragon::Calendar{2028,2,29,23,59,59},dragon::Calendar{2099,12,31,23,59,59}}){
    uint32_t expected=dragon::dateEpoch(date);regs[0]=0xa7;
    assert(boardWriteRtc(expected));assert((regs[0]&0xa2)==0);assert((regs[0]&5)==5);
    assert(boardReadRtc(epoch)&&epoch==expected);
  }
  regs[4]|=0x80;assert(!boardReadRtc(epoch)); // Oscillator stop cannot become valid by reading.
  uint32_t valid=dragon::dateEpoch({2028,2,29,12,34,56});assert(boardWriteRtc(valid));
  regs[10]=0xfa;assert(!boardReadRtc(epoch)); // Invalid year BCD must not decode as 1999.
  assert(boardWriteRtc(valid));regs[9]=0x13;assert(!boardReadRtc(epoch));
  assert(boardWriteRtc(valid));regs[0]|=0x80;assert(!boardReadRtc(epoch)); // External-clock test mode.
  assert(boardWriteRtc(valid));failRead=4;assert(!boardReadRtc(epoch));assert(!boardWriteRtc(valid));failRead=-1;
  failWrite=4;assert(!boardWriteRtc(valid));assert(!(regs[0]&0x20));failWrite=-1; // Restart after partial write failure.
  rtcAvailable=false;assert(!boardReadRtc(epoch));assert(!boardWriteRtc(valid));
  // Diagnostics must keep the fault bit visible and never rewrite the clock.
  const uint8_t snapshot[]{0x00,0x87,0x12,0xa5,0xd6,0x34,0x12,0x29,0x02,0x02,0x28};
  memcpy(regs,snapshot,sizeof(snapshot));readCalls=writeCalls=0;
  boardRtcDiagnostics();assert(readCalls==1&&lastReadReg==0&&lastReadCount==11&&writeCalls==0);
  assert(memcmp(regs,snapshot,sizeof(snapshot))==0);
  assert(strstr(serialText,"io=1 control=00 lost=1 control2=87 offset_raw=12 ram=a5 seconds_raw=d6 minutes_raw=34 hours_raw=12 day_raw=29 weekday_raw=02 month_raw=02 year_raw=28 uptime_ms=3210"));
  failRead=0;boardRtcDiagnostics();assert(writeCalls==0&&strstr(serialText,"io=0 error=register_read"));
  assert(!strstr(serialText,"seconds_raw=")&&!strstr(serialText,"lost=0")); // No fabricated valid snapshot.
  assert(memcmp(regs,snapshot,sizeof(snapshot))==0);failRead=-1;
  // Execute the production loop with physical GPIO transitions. USB injection
  // must not inflate the new physical-only diagnostic counters.
  std::fill(std::begin(pinLevels),std::end(pinLevels),1);
  nowMs=0;boardLoop();pinLevels[2]=0;nowMs=10;boardLoop();nowMs=36;boardLoop();
  assert(physicalInput.previous==1&&physicalInput.next==0&&deliveredSteps==-1);
  pinLevels[2]=1;nowMs=40;boardLoop();nowMs=66;boardLoop();
  pinLevels[3]=0;nowMs=80;boardLoop();nowMs=106;boardLoop();
  assert(physicalInput.next==1&&deliveredSteps==0);
  pinLevels[3]=1;pinLevels[28]=0;nowMs=120;boardLoop();nowMs=160;boardLoop();
  pinLevels[28]=1;nowMs=200;boardLoop();nowMs=240;boardLoop();
  assert(physicalInput.clicks==1&&physicalInput.backs==0&&deliveredClicks==1);
  pinLevels[28]=0;nowMs=300;boardLoop();nowMs=340;boardLoop();nowMs=1140;boardLoop();
  pinLevels[28]=1;nowMs=1180;boardLoop();nowMs=1220;boardLoop();
  assert(physicalInput.clicks==1&&physicalInput.backs==1&&deliveredBacks==1&&physicalInput.maxGapMs==800);
  inputEvent(4,dragon::RotaryPress::Click);
  assert(physicalInput.next==1&&physicalInput.clicks==1&&deliveredSteps==4&&deliveredClicks==2);
  physicalInput.maxGapMs=0;physicalInput.lastAt=0xfffffff0;nowMs=20;boardLoop();
  assert(physicalInput.maxGapMs==36);
}
'''
cpp = work / 'c5_board_test.cpp'
binary = work / ('c5_board_test.exe' if sys.platform == 'win32' else 'c5_board_test')
cpp.write_text(prelude + physical_stats + bcd + rtc + board_loop + tests)
subprocess.run([sys.executable, '-m', 'ziglang', 'c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                '-I', str(root / 'firmware/include'), str(cpp), '-o', str(binary)], check=True)
subprocess.run([str(binary)], check=True)
print('C5 input/RTC production tests PASS')
