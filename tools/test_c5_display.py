"""Exercise production C5 RGB666/window/async flush behavior without hardware."""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1]
work=root/'.work/c5-display-tests';work.mkdir(parents=True,exist_ok=True)
driver=(root/'firmware/src/board_c5_273.cpp').read_text()
def function(signature):
    start=driver.index(signature);brace=driver.index('{',start);end=brace+1;depth=1
    while depth:
        depth+=(driver[end]=='{')-(driver[end]=='}');end+=1
    return driver[start:end]
source=r'''
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>
constexpr int SCREEN_RES=320,DisplayPixels=320*8;
#define LV_COLOR_16_SWAP 1
using esp_lcd_panel_io_handle_t=void*;
struct esp_lcd_panel_io_event_data_t{};
struct lv_disp_drv_t{unsigned ready=0;};
struct lv_area_t{int x1,y1,x2,y2;};
struct lv_color_t{uint16_t full;};
using BaseType_t=int;constexpr int pdFALSE=0,pdTRUE=1,ESP_OK=0;
uint32_t now=100;uint32_t micros(){return now;}
bool semaphore=false;void* panelDone=nullptr;
int xSemaphoreTake(void*,int){bool result=semaphore;semaphore=false;return result;}
void xSemaphoreGiveFromISR(void*,BaseType_t*){semaphore=true;}
void lv_disp_flush_ready(lv_disp_drv_t* d){++d->ready;}
esp_lcd_panel_io_handle_t panelIo=nullptr;
uint8_t buffer[DisplayPixels*3]{};uint8_t* panelPixels=buffer;
bool displayTest=false,failCommand=false,failColor=false;
lv_disp_drv_t* volatile pendingFlush=nullptr;
volatile uint32_t flushStarted=0,flushLastUs=0,flushMaxUs=0,flushCount=0,flushPixels=0;
uint32_t flushErrors=0;
struct Command{uint8_t code;std::vector<uint8_t> bytes;};std::vector<Command> commands;
unsigned colorCalls=0;size_t bytesSent=0;const void* dmaBuffer=nullptr;
bool panelCommand(uint8_t command,const uint8_t* data,size_t n){commands.push_back({command,{data,data+n}});return !failCommand;}
int esp_lcd_panel_io_tx_color(void*,int command,const void* data,size_t bytes){assert(command==0x2c);++colorCalls;bytesSent=bytes;dmaBuffer=data;return failColor?-1:0;}
'''+function('bool panelTransferDone(')+function('void rgb666(')+function('bool panelSend(')+function('void panelFlush(')+r'''
int main(){
  uint8_t out[3];rgb666(0,out);assert(out[0]==0&&out[1]==0&&out[2]==0);
  rgb666(0xffff,out);assert(out[0]==0xfc&&out[1]==0xfc&&out[2]==0xfc);
  for(unsigned i=0;i<3;++i){rgb666(i==0?0xf800:i==1?0x07e0:0x001f,out);for(unsigned j=0;j<3;++j)assert(out[j]==(i==j?0xfc:0));}
  lv_color_t pixels[DisplayPixels];for(auto& p:pixels)p.full=__builtin_bswap16(uint16_t(0xf800));
  lv_disp_drv_t display;lv_area_t area{0,312,319,319};
  panelFlush(&display,&area,pixels);
  assert(display.ready==0&&pendingFlush==&display&&colorCalls==1&&bytesSent==7680&&dmaBuffer==buffer);
  assert(commands.size()==2&&commands[0].code==0x2a&&commands[0].bytes==std::vector<uint8_t>({0,0,1,63}));
  assert(commands[1].code==0x2b&&commands[1].bytes==std::vector<uint8_t>({1,56,1,63}));
  memset(pixels,0,sizeof(pixels));assert(buffer[0]==0xfc&&buffer[1]==0&&buffer[2]==0);
  now=1800;panelTransferDone(nullptr,nullptr,nullptr);assert(display.ready==1&&!pendingFlush&&flushLastUs==1700&&flushCount==1);
  commands.clear();area={312,0,319,319};panelFlush(&display,&area,pixels);assert(colorCalls==2&&bytesSent==7680&&buffer[0]==0);
  panelTransferDone(nullptr,nullptr,nullptr);
  area={0,0,319,8};panelFlush(&display,&area,pixels);assert(flushErrors==1&&colorCalls==2&&display.ready==3);
  area={-1,0,2,0};panelFlush(&display,&area,pixels);assert(flushErrors==2&&colorCalls==2);
  area={0,0,319,7};failCommand=true;panelFlush(&display,&area,pixels);assert(flushErrors==3&&!pendingFlush);failCommand=false;
  failColor=true;panelFlush(&display,&area,pixels);assert(flushErrors==4&&!pendingFlush);failColor=false;
  unsigned before=colorCalls;displayTest=true;panelFlush(&display,&area,pixels);assert(colorCalls==before&&!pendingFlush);
}
'''
cpp=work/'display.cpp';exe=work/'display.exe';cpp.write_text(source)
subprocess.run([sys.executable,'-m','ziglang','c++','-UNDEBUG','-std=c++17','-Wall','-Wextra','-Werror',str(cpp),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
print('PASS C5 RGB666 black/white/primaries, swapped LVGL pixels, exact square/rotated windows, DMA lifetime/completion and error paths')
