// SPDX-License-Identifier: MIT
// Waveshare ESP32-C5-LCD-2.73 adapter for Pirate Box / Dragon Detector.
// Wiring/registers checked against Waveshare's schematic and Arduino examples.
#include "app.h"
#ifdef BOARD_WS_C5_273
#include "board_c5_273.h"
#include "rotary_input.h"
#include "rotary_ui.h"
#include <Wire.h>
#include <Preferences.h>
#include <lvgl.h>
#include <esp_heap_caps.h>
#include <esp_lcd_panel_io.h>
#include <driver/spi_master.h>
#include <driver/sdspi_host.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#include <vfs_api.h>
#include <math.h>
#include <time.h>

C5CardFileSystem::C5CardFileSystem():fs::FS(fs::FSImplPtr(new VFSImpl())){}
void C5CardFileSystem::attach(uint64_t capacity){capacity_=capacity;_impl->mountpoint("/sdcard");}
uint64_t C5CardFileSystem::cardSize()const{return capacity_;}
uint64_t C5CardFileSystem::totalBytes(){uint64_t total=0,free=0;return capacity_&&esp_vfs_fat_info("/sdcard",&total,&free)==ESP_OK?total:0;}
uint64_t C5CardFileSystem::usedBytes(){uint64_t total=0,free=0;return capacity_&&esp_vfs_fat_info("/sdcard",&total,&free)==ESP_OK?total-free:0;}
C5CardFileSystem C5CardFS;

namespace {
constexpr uint8_t Expander=0x24,RtcAddress=0x51,WeatherAddress=0x70,ImuAddress=0x6b;
constexpr uint8_t ModeRegister=2,OutputRegister=3,InputRegister=4,PwmRegister=5,AdcRegister=6;
constexpr unsigned DisplayRows=8,DisplayPixels=SCREEN_RES*DisplayRows;
esp_lcd_panel_io_handle_t panelIo=nullptr;
uint8_t* panelPixels=nullptr;
sdmmc_card_t* nativeCard=nullptr;
bool spiReady=false,displayTest=false;
SemaphoreHandle_t panelDone=nullptr;
lv_disp_drv_t* volatile pendingFlush=nullptr;
volatile uint32_t flushStarted=0,flushLastUs=0,flushMaxUs=0,flushCount=0,flushPixels=0;
uint32_t flushErrors=0;
lv_disp_draw_buf_t drawBuffer;
lv_disp_drv_t displayDriver;
C5SensorStatus sensors;
uint16_t outputState=0xffff;
piratebox::InputConfig inputConfig;
piratebox::DirectionButton leftKey,rightKey,externalDirections[4];
dragon::RotaryButton bootKey,externalClick;
dragon::DetentDecoder encoder;
bool encoderSynchronized=false,uiReady=false,imuReady=false,sensorsStarted=false;
uint32_t externalPollAt=0,lastSlowSample=0,weatherStepAt=0,weatherCycleAt=0;
uint8_t weatherPhase=0;
uint32_t previousSteps=0,nextSteps=0,clicks=0,backs=0;
struct PhysicalInputStats {
  uint32_t loops=0,lastAt=0,maxGapMs=0,previous=0,next=0,clicks=0,backs=0;
} physicalInput;

bool panelTransferDone(esp_lcd_panel_io_handle_t,esp_lcd_panel_io_event_data_t*,void*){
  uint32_t elapsed=uint32_t(micros()-flushStarted);flushLastUs=elapsed;
  if(elapsed>flushMaxUs)flushMaxUs=elapsed;
  flushCount=flushCount+1;
  auto* driver=pendingFlush;pendingFlush=nullptr;
  if(driver)lv_disp_flush_ready(driver);
  BaseType_t wake=pdFALSE;xSemaphoreGiveFromISR(panelDone,&wake);return wake==pdTRUE;
}
bool panelCommand(uint8_t command,const uint8_t* data=nullptr,size_t count=0){
  return esp_lcd_panel_io_tx_param(panelIo,command,data,count)==ESP_OK;
}
void panelBegin(){
  spi_bus_config_t bus{};bus.mosi_io_num=7;bus.miso_io_num=5;bus.sclk_io_num=6;
  bus.quadwp_io_num=-1;bus.quadhd_io_num=-1;bus.max_transfer_sz=DisplayPixels*3;
  ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST,&bus,SPI_DMA_CH_AUTO));spiReady=true;
  panelDone=xSemaphoreCreateBinary();
  panelPixels=(uint8_t*)heap_caps_malloc(DisplayPixels*3,MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL);
  if(!panelDone||!panelPixels){Serial.println("FATAL C5 display DMA allocation");abort();}
  esp_lcd_panel_io_spi_config_t io{};io.dc_gpio_num=4;io.cs_gpio_num=C5_TFT_CS;
  io.pclk_hz=60000000;io.lcd_cmd_bits=8;io.lcd_param_bits=8;io.spi_mode=0;io.trans_queue_depth=1;
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST,&io,&panelIo));
  esp_lcd_panel_io_callbacks_t callbacks{};callbacks.on_color_trans_done=panelTransferDone;
  ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(panelIo,&callbacks,nullptr));
  if(!panelCommand(0x01)){Serial.println("FATAL C5 panel reset");abort();}delay(120);
  // Waveshare ESP32-C5-LCD-2.73 Arduino 3.3.10 example, esp_lcd_ili9488.c.
  // Its power, gamma and interface values differ from a generic 320x480 panel.
  struct Init {uint8_t command,size,data[15];};
  static const Init commands[]{
    {0xc0,2,{0x10,0x12}},{0xc1,1,{0x66}},{0xc5,3,{0x00,0x45,0x80}},
    {0x36,1,{0x48}},{0x21,0,{}},{0x3a,1,{0x66}},{0xb0,1,{0x80}},
    {0xb1,1,{0xa0}},{0xb4,1,{0x02}},{0xb6,3,{0x00,0x02,0x3b}},
    {0xe0,15,{0x00,0x08,0x19,0x05,0x16,0x09,0x35,0x47,0x4a,0x04,0x0d,0x0c,0x30,0x33,0x0f}},
    {0xe1,15,{0x00,0x08,0x19,0x05,0x16,0x09,0x35,0x47,0x4a,0x04,0x0d,0x0c,0x30,0x33,0x0f}},
    {0xe9,1,{0x00}},{0xf7,4,{0xa9,0x51,0x2c,0x82}}
  };
  for(const auto& command:commands)if(!panelCommand(command.command,command.data,command.size)){Serial.println("FATAL C5 panel init");abort();}
  if(!panelCommand(0x11)){Serial.println("FATAL C5 panel sleep-out");abort();}delay(120);
  if(!panelCommand(0x29)){Serial.println("FATAL C5 panel display-on");abort();}delay(100);
}
void rgb666(uint16_t color,uint8_t* out){
  // Same 5/6/5 to 6/6/6 bit expansion as Waveshare's reference driver.
  out[0]=uint8_t(((color&0xf800)>>8)|((color&0x8000)>>13));
  out[1]=uint8_t((color&0x07e0)>>3);
  out[2]=uint8_t(((color&0x001f)<<3)|((color&0x0010)>>2));
}
bool panelSend(int x,int y,int width,int height,lv_disp_drv_t* driver){
  if(!driver)flushStarted=micros();
  const uint16_t right=x+width-1,bottom=y+height-1;
  const uint8_t columns[]{uint8_t(x>>8),uint8_t(x),uint8_t(right>>8),uint8_t(right)};
  const uint8_t rows[]{uint8_t(y>>8),uint8_t(y),uint8_t(bottom>>8),uint8_t(bottom)};
  if(!panelCommand(0x2a,columns,4)||!panelCommand(0x2b,rows,4))return false;
  while(xSemaphoreTake(panelDone,0)==pdTRUE){}
  pendingFlush=driver;flushPixels=flushPixels+width*height;
  if(esp_lcd_panel_io_tx_color(panelIo,0x2c,panelPixels,size_t(width)*height*3)!=ESP_OK){pendingFlush=nullptr;return false;}
  return true;
}
void panelFlush(lv_disp_drv_t* driver,const lv_area_t* area,lv_color_t* pixels){
  if(displayTest){lv_disp_flush_ready(driver);return;}
  flushStarted=micros();
  const int width=area->x2-area->x1+1,height=area->y2-area->y1+1;
  const size_t count=size_t(width)*height;
  if(width<=0||height<=0||count>DisplayPixels||area->x1<0||area->y1<0||area->x2>=SCREEN_RES||area->y2>=SCREEN_RES){
    ++flushErrors;lv_disp_flush_ready(driver);return;
  }
  for(size_t i=0;i<count;++i){uint16_t color=pixels[i].full;
#if LV_COLOR_16_SWAP
    color=__builtin_bswap16(color);
#endif
    rgb666(color,panelPixels+i*3);
  }
  if(!panelSend(area->x1,area->y1,width,height,driver)){++flushErrors;lv_disp_flush_ready(driver);}
}
bool panelDiagnostic(bool bars){
  // Called only by an explicit console command. Suspend ordinary flushes until
  // resume, and use the same conversion/DMA path as the production display.
  if(!panelIo)return false;
  lv_refr_now(lv_disp_get_default());
  if(pendingFlush&&xSemaphoreTake(panelDone,pdMS_TO_TICKS(1000))!=pdTRUE)return false;
  displayTest=true;
  constexpr uint16_t colors[]{0xf800,0x07e0,0x001f,0xffff,0x0000,0x8410};
  for(int y=0;y<SCREEN_RES;y+=DisplayRows){
    const int height=std::min<int>(DisplayRows,SCREEN_RES-y);
    for(int row=0;row<height;++row)for(int x=0;x<SCREEN_RES;++x)
      rgb666(bars?colors[unsigned(x)*6/SCREEN_RES]:0,panelPixels+(row*SCREEN_RES+x)*3);
    if(!panelSend(0,y,SCREEN_RES,height,nullptr)||xSemaphoreTake(panelDone,pdMS_TO_TICKS(1000))!=pdTRUE){++flushErrors;return false;}
  }
  return true;
}

bool readRegisters(uint8_t address,uint8_t reg,uint8_t* out,size_t count){
  Wire.beginTransmission(address);Wire.write(reg);
  if(Wire.endTransmission(false)!=0||Wire.requestFrom(address,uint8_t(count))!=count)return false;
  for(size_t i=0;i<count;++i)out[i]=Wire.read();
  return true;
}
bool writeRegisters(uint8_t address,uint8_t reg,const uint8_t* bytes,size_t count){
  Wire.beginTransmission(address);Wire.write(reg);Wire.write(bytes,count);
  return Wire.endTransmission()==0;
}
bool writeRegister(uint8_t address,uint8_t reg,uint8_t value){return writeRegisters(address,reg,&value,1);}
bool writeWord(uint8_t reg,uint16_t value){
  const uint8_t bytes[]{uint8_t(value),uint8_t(value>>8)};
  return writeRegisters(Expander,reg,bytes,2);
}
bool readWord(uint8_t reg,uint16_t& value){
  uint8_t bytes[2]{};if(!readRegisters(Expander,reg,bytes,2))return false;
  value=uint16_t(bytes[0])|(uint16_t(bytes[1])<<8);return true;
}
int fromBcd(uint8_t value){return (value&15)<=9&&(value>>4)<=9?(value>>4)*10+(value&15):-1;}
uint8_t toBcd(int value){return uint8_t(value/10*16+value%10);}
uint8_t crc8(const uint8_t* bytes){
  uint8_t crc=0xff;
  for(unsigned i=0;i<2;++i){crc^=bytes[i];for(unsigned bit=0;bit<8;++bit)crc=crc&0x80?uint8_t((crc<<1)^0x31):uint8_t(crc<<1);}
  return crc;
}
bool weatherCommand(uint16_t command){
  Wire.beginTransmission(WeatherAddress);Wire.write(uint8_t(command>>8));Wire.write(uint8_t(command));
  return Wire.endTransmission()==0;
}
void weatherFailed(){
  sensors.weatherValid=false;++sensors.weatherErrors;
  weatherCommand(0xb098);weatherPhase=0;weatherCycleAt=millis();
}
void sampleWeather(uint32_t now){
  if(weatherPhase==0){
    if(uint32_t(now-weatherCycleAt)<2000)return;
    if(!weatherCommand(0x3517)){weatherFailed();return;}
    weatherStepAt=now;weatherPhase=1;return;
  }
  if(weatherPhase==1){
    if(uint32_t(now-weatherStepAt)<1)return;
    if(!weatherCommand(0x7866)){weatherFailed();return;}
    weatherStepAt=now;weatherPhase=2;return;
  }
  if(uint32_t(now-weatherStepAt)<20)return;
  uint8_t bytes[6]{};
  if(Wire.requestFrom(WeatherAddress,uint8_t(6))!=6){weatherFailed();return;}
  for(auto& byte:bytes)byte=Wire.read();
  weatherCommand(0xb098);weatherPhase=0;weatherCycleAt=now;
  if(crc8(bytes)!=bytes[2]||crc8(bytes+3)!=bytes[5]){sensors.weatherValid=false;++sensors.weatherErrors;return;}
  sensors.temperatureC=-45.f+175.f*float((uint16_t(bytes[0])<<8)|bytes[1])/65535.f;
  sensors.humidityPercent=100.f*float((uint16_t(bytes[3])<<8)|bytes[4])/65535.f;
  sensors.weatherAt=now;sensors.weatherValid=true;
}
void beginImu(){
  uint8_t id=0;imuReady=false;
  if(!readRegisters(ImuAddress,0,&id,1)||id!=5)return;
  // Auto increment, disable before configuring; +/-8g and +/-512dps at 125Hz.
  imuReady=writeRegister(ImuAddress,2,0x60)&&writeRegister(ImuAddress,8,0)&&
    writeRegister(ImuAddress,3,0x26)&&writeRegister(ImuAddress,4,0x56)&&
    writeRegister(ImuAddress,6,0x11)&&writeRegister(ImuAddress,8,3);
}
void sampleSlowSensors(uint32_t now){
  if(uint32_t(now-lastSlowSample)<500)return;
  lastSlowSample=now;
  uint16_t raw=0;sensors.batteryMillivolts=-1;
  if(sensors.expanderReady&&readWord(AdcRegister,raw)&&raw>0&&raw<=4095){
    int mv=(uint32_t(raw)*9900+2047)/4095;
    if(mv>=2500&&mv<=4400)sensors.batteryMillivolts=mv;
  }
  if(!imuReady)return;
  uint8_t bytes[12]{};
  if(!readRegisters(ImuAddress,53,bytes,sizeof(bytes))){sensors.imuValid=false;++sensors.imuErrors;return;}
  for(unsigned i=0;i<3;++i){
    sensors.accelerationG[i]=int16_t(uint16_t(bytes[i*2])|(uint16_t(bytes[i*2+1])<<8))/4096.f;
    sensors.rotationDps[i]=int16_t(uint16_t(bytes[6+i*2])|(uint16_t(bytes[7+i*2])<<8))/64.f;
  }
  sensors.imuAt=now;sensors.imuValid=true;
}
void inputEvent(int steps,dragon::RotaryPress press=dragon::RotaryPress::None){
  if(steps<0)previousSteps+=-steps;else nextSteps+=steps;
  bool click=press==dragon::RotaryPress::Click,back=press==dragon::RotaryPress::Back;
  clicks+=click;backs+=back;
  if(uiReady)rotaryUiInput(steps,click,back);
}
void sampleExternalInput(uint32_t now){
  if(inputConfig.mode==piratebox::InputMode::Off||uint32_t(now-externalPollAt)<2)return;
  externalPollAt=now;uint16_t state=0xffff;
  if(!readWord(InputRegister,state)){
    ++sensors.inputErrors;encoderSynchronized=false;externalClick=dragon::RotaryButton{};
    for(auto& key:externalDirections)key=piratebox::DirectionButton{};
    return;
  }
  auto down=[&](unsigned at){return (state&(1U<<inputConfig.pins[at]))==0;};
  if(inputConfig.mode==piratebox::InputMode::Encoder){
    uint8_t ab=(down(0)?0:1)|(down(1)?0:2);
    if(!encoderSynchronized){encoder.reset(ab);encoderSynchronized=true;}
    inputEvent(encoder.update(ab),externalClick.update(down(2),now));
  }else{
    int steps=0;
    for(unsigned i=0;i<4;++i)if(externalDirections[i].update(down(i),now))steps+=(i==0||i==2)?-1:1;
    inputEvent(steps,externalClick.update(down(4),now));
  }
}
void printInput(){
  const char* names[]{"off","encoder","joystick","buttons"};
  Serial.printf("INPUT_STATUS mode=%s exio=%u,%u,%u,%u,%u KeyL=2:previous KeyR=3:next BOOT=28:select/hold-back previous=%lu next=%lu clicks=%lu backs=%lu errors=%lu raw_l=%d raw_r=%d raw_boot=%d physical_previous=%lu physical_next=%lu physical_clicks=%lu physical_backs=%lu board_loops=%lu max_gap_ms=%lu ui_ready=%u\n",
    names[uint8_t(inputConfig.mode)],inputConfig.pins[0],inputConfig.pins[1],inputConfig.pins[2],inputConfig.pins[3],inputConfig.pins[4],previousSteps,nextSteps,clicks,backs,sensors.inputErrors,
    digitalRead(C5_KEY_L),digitalRead(C5_KEY_R),digitalRead(BTN_PIN),physicalInput.previous,physicalInput.next,physicalInput.clicks,physicalInput.backs,physicalInput.loops,physicalInput.maxGapMs,uiReady);
}
}

void boardBegin(){
  pinMode(C5_KEY_L,INPUT_PULLUP);pinMode(C5_KEY_R,INPUT_PULLUP);pinMode(BTN_PIN,INPUT_PULLUP);
  pinMode(C5_SD_CS,OUTPUT);digitalWrite(C5_SD_CS,HIGH);pinMode(C5_TFT_CS,OUTPUT);digitalWrite(C5_TFT_CS,HIGH);
  Wire.begin(C5_I2C_SDA,C5_I2C_SCL,400000);Wire.setTimeOut(20);
  // EXIO4..14 are always high-impedance inputs: a connected switch cannot
  // short a driven output during startup or while expansion input is disabled.
  sensors.expanderReady=writeWord(ModeRegister,0x8007)&&writeWord(OutputRegister,outputState);
  if(sensors.expanderReady){
    outputState&=~uint16_t(1);bool low=writeWord(OutputRegister,outputState);delay(20);
    outputState|=1;sensors.expanderReady=writeWord(OutputRegister,outputState)&&low;delay(150);
  }
  Preferences prefs;
  if(prefs.begin("pb-input",true)){
    uint8_t bytes[6]{};
    if(prefs.getBytesLength("config")==sizeof(bytes)&&prefs.getBytes("config",bytes,sizeof(bytes))==sizeof(bytes)){
      piratebox::InputConfig saved;saved.mode=piratebox::InputMode(bytes[0]);
      memcpy(saved.pins,bytes+1,5);if(piratebox::validInputConfig(saved))inputConfig=saved;
    }
    prefs.end();
  }
}
void boardLoop(){
  uint32_t now=millis();
  if(physicalInput.loops)physicalInput.maxGapMs=std::max(physicalInput.maxGapMs,uint32_t(now-physicalInput.lastAt));
  physicalInput.lastAt=now;++physicalInput.loops;
  bool next=rightKey.update(digitalRead(C5_KEY_R)==LOW,now);
  bool previous=leftKey.update(digitalRead(C5_KEY_L)==LOW,now);
  auto press=bootKey.update(digitalRead(BTN_PIN)==LOW,now);
  // These counts exclude USB injection and future expansion controls.
  physicalInput.next+=next;physicalInput.previous+=previous;
  physicalInput.clicks+=press==dragon::RotaryPress::Click;
  physicalInput.backs+=press==dragon::RotaryPress::Back;
  inputEvent(int(next)-int(previous),press);
  if(sensors.expanderReady)sampleExternalInput(now);
  if(sensorsStarted){sampleWeather(now);sampleSlowSensors(now);}
}
void set_brightness(uint8_t percent){
  if(sensors.expanderReady)writeRegister(Expander,PwmRegister,uint16_t(constrain(percent,0,100))*247/100);
}
void scr_lvgl_init(){
  panelBegin();lv_init();
  auto* pixels=(lv_color_t*)heap_caps_malloc(DisplayPixels*sizeof(lv_color_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!pixels){Serial.println("FATAL C5 LVGL tile allocation");abort();}
  lv_disp_draw_buf_init(&drawBuffer,pixels,nullptr,DisplayPixels);
  lv_disp_drv_init(&displayDriver);displayDriver.hor_res=SCREEN_RES;displayDriver.ver_res=SCREEN_RES;
  displayDriver.draw_buf=&drawBuffer;
  displayDriver.flush_cb=panelFlush;
  lv_disp_drv_register(&displayDriver);
  // UI owns creation and refresh of the dynamic focus group.
  uiReady=true;set_brightness(75);
  Serial.printf("C5_DISPLAY ILI9488=%dx%d transport=IDF_DMA_RGB666 clock=60MHz rows=%u PSRAM=%u KeyL=previous KeyR=next BOOT=select\n",SCREEN_RES,SCREEN_RES,DisplayRows,ESP.getPsramSize());
}
bool boardMountCard(){
  if(nativeCard)return true;
  if(!spiReady)return false;
  // Both devices use IDF's SPI2 arbitration. Never initialize a second Arduino
  // SPI controller over the same pins, and never format a failed card mount.
  esp_vfs_fat_sdmmc_mount_config_t mount{};mount.format_if_mount_failed=false;
  mount.max_files=12;mount.allocation_unit_size=512;
  sdspi_device_config_t slot=SDSPI_DEVICE_CONFIG_DEFAULT();slot.host_id=SPI2_HOST;slot.gpio_cs=(gpio_num_t)C5_SD_CS;
  for(int frequency:{20000,4000,1000}){
    sdmmc_host_t host=SDSPI_HOST_DEFAULT();host.slot=SPI2_HOST;host.max_freq_khz=frequency;
    esp_err_t result=esp_vfs_fat_sdspi_mount("/sdcard",&host,&slot,&mount,&nativeCard);
    Serial.printf("C5_SD native_spi2 frequency_khz=%d result=%s heap=%u\n",frequency,esp_err_to_name(result),ESP.getFreeHeap());
    if(result==ESP_OK&&nativeCard){
      C5CardFS.attach(uint64_t(nativeCard->csd.capacity)*nativeCard->csd.sector_size);
      return true;
    }
    nativeCard=nullptr;delay(30);
  }
  return false;
}
int batteryPercent(){
  int mv=sensors.batteryMillivolts;return mv<0?-1:constrain((mv-3300)*100/900,0,100);
}
bool batteryCharging(){return false;} // Board exposes voltage, not a charge-state signal.
void boardSensorsBegin(){
  uint8_t control=0;rtcAvailable=false;
  for(unsigned attempt=0;attempt<3&&!rtcAvailable;++attempt){
    rtcAvailable=readRegisters(RtcAddress,0,&control,1);if(!rtcAvailable)delay(5);
  }
  if(!sensorsStarted){beginImu();weatherCycleAt=millis()-2000;lastSlowSample=millis()-500;sensorsStarted=true;}
}
bool boardReadRtc(uint32_t& epoch){
  uint8_t control=0,bytes[7]{};
  if(!rtcAvailable||!readRegisters(RtcAddress,0,&control,1)||(control&0xa2)||
     !readRegisters(RtcAddress,4,bytes,7)||(bytes[0]&0x80))return false;
  int year=fromBcd(bytes[6]);if(year<0)return false;
  dragon::Calendar date{2000+year,fromBcd(bytes[5]&31),fromBcd(bytes[3]&63),fromBcd(bytes[2]&63),fromBcd(bytes[1]&127),fromBcd(bytes[0]&127)};
  epoch=dragon::dateEpoch(date);return epoch!=0;
}
bool boardWriteRtc(uint32_t epoch){
  if(!rtcAvailable||!dragon::validEpoch(epoch))return false;
  time_t value=epoch;struct tm date{};gmtime_r(&value,&date);
  if(date.tm_year<100||date.tm_year>199)return false;
  uint8_t control=0;if(!readRegisters(RtcAddress,0,&control,1))return false;
  control=(control&5)|0x20; // preserve calibration; exit external-test/12h mode
  if(!writeRegister(RtcAddress,0,control))return false;
  uint8_t bytes[]{toBcd(date.tm_sec),toBcd(date.tm_min),toBcd(date.tm_hour),toBcd(date.tm_mday),uint8_t(date.tm_wday),toBcd(date.tm_mon+1),toBcd(date.tm_year%100)};
  bool written=writeRegisters(RtcAddress,4,bytes,7);
  control&=~0x20;bool restarted=writeRegister(RtcAddress,0,control);
  uint32_t check=0;return written&&restarted&&boardReadRtc(check)&&check>=epoch&&check-epoch<=1;
}
void boardRtcDiagnostics(){
  // One coherent, read-only snapshot. Raw BCD (including OS) distinguishes a
  // ticking clock with a latched fault from a stopped clock or a reset date.
  uint8_t raw[11]{};
  if(!readRegisters(RtcAddress,0,raw,sizeof(raw))){
    Serial.printf("CLOCK_DIAGNOSTICS chip=PCF85063 io=0 error=register_read uptime_ms=%lu\n",static_cast<unsigned long>(millis()));return;
  }
  Serial.printf("CLOCK_DIAGNOSTICS chip=PCF85063 io=1 control=%02x lost=%u control2=%02x offset_raw=%02x ram=%02x seconds_raw=%02x minutes_raw=%02x hours_raw=%02x day_raw=%02x weekday_raw=%02x month_raw=%02x year_raw=%02x uptime_ms=%lu\n",
    raw[0],(raw[4]&0x80)!=0,raw[1],raw[2],raw[3],raw[4],raw[5],raw[6],raw[7],raw[8],raw[9],raw[10],static_cast<unsigned long>(millis()));
}
C5SensorStatus boardC5Sensors(){
  C5SensorStatus status=sensors;const uint32_t now=millis();
  status.weatherValid=status.weatherValid&&uint32_t(now-status.weatherAt)<10000;
  status.imuValid=status.imuValid&&uint32_t(now-status.imuAt)<3000;
  return status;
}
bool boardReadWeather(float& temperatureC,float& humidityPercent){
  auto status=boardC5Sensors();if(!status.weatherValid)return false;
  temperatureC=status.temperatureC;humidityPercent=status.humidityPercent;return true;
}
piratebox::InputConfig boardC5InputConfig(){return inputConfig;}
bool boardC5SetInput(const piratebox::InputConfig& config){
  if(!piratebox::validInputConfig(config))return false;
  if(config.mode!=piratebox::InputMode::Off&&!sensors.expanderReady)return false;
  uint8_t bytes[6]{uint8_t(config.mode)};memcpy(bytes+1,config.pins,5);
  Preferences prefs;if(!prefs.begin("pb-input",false))return false;
  bool saved=prefs.putBytes("config",bytes,sizeof(bytes))==sizeof(bytes);prefs.end();
  if(!saved)return false;
  inputConfig=config;encoderSynchronized=false;externalClick=dragon::RotaryButton{};
  for(auto& key:externalDirections)key=piratebox::DirectionButton{};
  return true;
}
bool boardC5Command(const String& command){
  if(command=="c5 display status"){
    Serial.printf("C5_DISPLAY_STATUS transport=IDF_DMA_RGB666 width=%u height=%u rows=%u test=%u flushes=%lu pixels=%lu last_us=%lu max_us=%lu errors=%lu internal_free=%u\n",SCREEN_RES,SCREEN_RES,DisplayRows,displayTest,flushCount,flushPixels,flushLastUs,flushMaxUs,flushErrors,ESP.getFreeHeap());return true;
  }
  if(command=="c5 display bars"||command=="c5 display black"){
    bool bars=command.endsWith("bars");bool ok=panelDiagnostic(bars);
    Serial.printf("C5_DISPLAY_TEST %s %s; use c5 display resume to return\n",bars?"left-to-right RED GREEN BLUE WHITE BLACK GRAY":"BLACK CLEAR",ok?"OK":"FAILED");return true;
  }
  if(command=="c5 display resume"){
    displayTest=false;lv_obj_invalidate(lv_scr_act());lv_obj_invalidate(lv_layer_top());
    Serial.println("C5_DISPLAY_TEST resumed");return true;
  }
  // Explicit USB diagnostic actions exercise the same focus/navigation path
  // as physical buttons. They are never generated by background services.
  if(command.startsWith("c5 input ")){
    if(command=="c5 input next")inputEvent(1);
    else if(command=="c5 input previous")inputEvent(-1);
    else if(command=="c5 input select")inputEvent(0,dragon::RotaryPress::Click);
    else if(command=="c5 input back")inputEvent(0,dragon::RotaryPress::Back);
    else {Serial.println("C5_INPUT_ERROR next|previous|select|back");return true;}
    Serial.println("C5_INPUT_OK");return true;
  }
  if(command=="input status"||command=="pbkeys"){printInput();return true;}
  if(command.startsWith("input ")){
    piratebox::InputConfig config;int p[5]{};char mode[12]{};
    bool valid=command=="input off";
    if(!valid){
      sscanf(command.c_str(),"input %11s",mode);
      config.mode=strcmp(mode,"encoder")==0?piratebox::InputMode::Encoder:
        strcmp(mode,"joystick")==0?piratebox::InputMode::Joystick:
        strcmp(mode,"buttons")==0?piratebox::InputMode::Buttons:piratebox::InputMode::Off;
      const unsigned count=config.mode==piratebox::InputMode::Encoder?3:5;
      // sscanf must consume the entire command, including non-numeric tails.
      int consumed=0;
      if(count==3)valid=sscanf(command.c_str(),"input %11s %d %d %d %n",mode,&p[0],&p[1],&p[2],&consumed)==4;
      else valid=sscanf(command.c_str(),"input %11s %d %d %d %d %d %n",mode,&p[0],&p[1],&p[2],&p[3],&p[4],&consumed)==6;
      valid=valid&&consumed==int(command.length())&&config.mode!=piratebox::InputMode::Off;
      for(unsigned i=0;i<count&&valid;++i){valid=p[i]>=4&&p[i]<=14;config.pins[i]=uint8_t(p[i]);}
    }
    Serial.println(valid&&boardC5SetInput(config)?"INPUT_OK":"INPUT_ERROR use off | encoder A B SELECT | joystick UP DOWN LEFT RIGHT SELECT | buttons UP DOWN LEFT RIGHT SELECT; unique EXIO4..14 only");
    return true;
  }
  if(command=="board health"||command=="pbhealth"||command=="weather status"||command=="imu status"){
    auto status=boardC5Sensors();uint32_t epoch=0;bool clockValid=boardReadRtc(epoch);
    Serial.printf("BOARD_HEALTH expander=%u sd=%u rtc=%u clock_valid=%u battery_mv=%d psram=%u heap=%u\n",status.expanderReady,sdReady,rtcAvailable,clockValid,status.batteryMillivolts,ESP.getPsramSize(),ESP.getFreeHeap());
    Serial.printf("WEATHER_STATUS valid=%u temperature_c=%.2f humidity_percent=%.2f age_ms=%lu errors=%lu\n",status.weatherValid,status.temperatureC,status.humidityPercent,uint32_t(millis()-status.weatherAt),status.weatherErrors);
    Serial.printf("IMU_STATUS valid=%u accel_g=%.3f,%.3f,%.3f gyro_dps=%.2f,%.2f,%.2f errors=%lu\n",status.imuValid,status.accelerationG[0],status.accelerationG[1],status.accelerationG[2],status.rotationDps[0],status.rotationDps[1],status.rotationDps[2],status.imuErrors);
    return true;
  }
  return false;
}
#endif
