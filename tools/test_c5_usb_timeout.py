"""Exercise production C5 USB timeout guard and binary transfer retry behavior."""
from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
main = (root / 'firmware/src/main.cpp').read_text()
guard = re.search(r'struct NativeUsbCommandTimeout \{.*?\n\};', main, re.S).group()
transfer = main[main.index('static bool serialBytes('):main.index('static lv_color_t* capturePixels')]
command = main[main.index('static void command('):main.index('void setup()')]
assert command.index('NativeUsbCommandTimeout commandTimeout;') < command.index('if(boardC5Command(c))return;')
setup = main[main.index('void setup()'):main.index('void loop()')]
assert setup.index('Serial.begin(115200)') < setup.index('Serial.setTxTimeoutMs(0);') < setup.index('Serial.println(')
assert '#ifdef BOARD_WS_C5_273\n  NativeUsbCommandTimeout commandTimeout;' in command

work = root / '.work/c5-usb-tests'
work.mkdir(parents=True, exist_ok=True)
source = work / 'usb_timeout.cpp'
source.write_text(r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
static uint32_t now=0,ticks=0;
uint32_t millis(){return now;}
void delay(uint32_t ms){now+=ms;}
void radioTransferTick(){++ticks;}
struct SerialMock {
  unsigned timeout=0,calls=0,blocked=0;
  bool stalled=false;
  std::vector<uint8_t> sent;
  void setTxTimeoutMs(unsigned value){timeout=value;}
  size_t availableForWrite(){++calls;return stalled||calls%4==0?0:71;}
  size_t write(const uint8_t* data,size_t count){
    assert(timeout==100); // Explicit binary transfers retain the old budget.
    if(calls%3==0)return 0; // Retry short/transiently failed writes.
    size_t n=std::min<size_t>(count,17);sent.insert(sent.end(),data,data+n);return n;
  }
} Serial;
''' + guard + '\n' + transfer + r'''
static bool early(bool boardHandled){
  NativeUsbCommandTimeout timeout;
  assert(Serial.timeout==100);
  if(boardHandled)return true;
  return false;
}
int main(){
  assert(Serial.timeout==0);
  assert(early(true)&&Serial.timeout==0);
  assert(!early(false)&&Serial.timeout==0);
  std::vector<uint8_t> payload(4096);for(unsigned i=0;i<payload.size();++i)payload[i]=uint8_t(i);
  {NativeUsbCommandTimeout timeout;assert(serialBytes(payload.data(),payload.size()));}
  assert(Serial.timeout==0&&Serial.sent==payload&&ticks>0);
  Serial.stalled=true;now=0;ticks=0;auto count=Serial.sent.size();
  {NativeUsbCommandTimeout timeout;assert(!serialBytes(payload.data(),payload.size()));}
  assert(Serial.timeout==0&&Serial.sent.size()==count&&now>5000&&now<5010&&ticks>=5000);
  // Unsigned elapsed arithmetic still terminates across millis rollover.
  now=UINT32_MAX-100;
  {NativeUsbCommandTimeout timeout;assert(!serialBytes(payload.data(),payload.size()));}
  assert(Serial.timeout==0&&now<5010);
  puts("PASS C5 USB: early-return timeout restoration, exact binary bytes through short writes/backpressure, bounded stalled-host transfer, millis wrap");
}
''')
exe = work / ('usb_timeout.exe' if sys.platform == 'win32' else 'usb_timeout')
subprocess.run([sys.executable, '-m', 'ziglang', 'c++', '-UNDEBUG', '-std=c++17', '-O1', str(source), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
