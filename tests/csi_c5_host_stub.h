#pragma once
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <string>
#include <vector>
#include <cstdarg>
using esp_err_t=int;
constexpr int ESP_OK=0,ESP_ERR_NOT_SUPPORTED=0x106,ESP_FAIL=0x101;
using wifi_mode_t=int;constexpr int WIFI_MODE_NULL=0,WIFI_MODE_STA=1;
using wifi_band_mode_t=int;constexpr int WIFI_BAND_MODE_2G_ONLY=1,WIFI_BAND_MODE_5G_ONLY=2,WIFI_BAND_MODE_AUTO=3;
using wifi_ps_type_t=int;constexpr int WIFI_PS_NONE=0,WIFI_PS_MIN_MODEM=1;
using wifi_second_chan_t=int;constexpr int WIFI_SECOND_CHAN_NONE=0;
constexpr int WIFI_IF_STA=0,WIFI_BW_HT20=20,WIFI_BW_HT40=40;
constexpr unsigned RX_BB_FORMAT_11G=1,RX_BB_FORMAT_HT=2,RX_BB_FORMAT_HE_SU=4;
struct wifi_bandwidths_t {int ghz_2g=0,ghz_5g=0;};
struct wifi_csi_config_t {
 unsigned enable=0,acquire_csi_legacy=0,acquire_csi_force_lltf=0,acquire_csi_ht20=0,acquire_csi_ht40=0,
 acquire_csi_vht=0,acquire_csi_su=0,acquire_csi_mu=0,acquire_csi_dcm=0,acquire_csi_beamformed=0,
 acquire_csi_he_stbc_mode=0,val_scale_cfg=0,dump_ack_en=0,lltf_bit_mode=0,reserved=0;
};
struct wifi_pkt_rx_ctrl_t {
 unsigned channel=6,cur_bb_format=RX_BB_FORMAT_11G,rx_state=0,rxend_state=0,rx_channel_estimate_info_vld=1,second=0;
 int rssi=-45;
};
struct wifi_csi_info_t {
 wifi_pkt_rx_ctrl_t rx_ctrl{};uint8_t mac[6]{};bool first_word_invalid=false;int8_t* buf=nullptr;uint16_t len=0;
};
using wifi_csi_cb_t=void(*)(void*,wifi_csi_info_t*);
class String:public std::string {
 public:using std::string::string;
 bool startsWith(const char* p)const{return rfind(p,0)==0;}
};
struct FakeSerial {
 std::string text;
 void println(const char* p){text+=p;text+='\n';}
 void printf(const char* format,...){char buffer[2048];va_list args;va_start(args,format);vsnprintf(buffer,sizeof(buffer),format,args);va_end(args);text+=buffer;}
} Serial;
size_t strlcpy(char* out,const char* value,size_t cap){size_t n=strlen(value);if(cap){size_t k=std::min(cap-1,n);memcpy(out,value,k);out[k]=0;}return n;}
uint32_t gNow=50000;uint32_t millis(){return gNow;}
const char* esp_err_to_name(int){return "FAKE_ERROR";}
bool gFailAlloc=false,gFailQueue=false;
void* ps_malloc(size_t size){return gFailAlloc?nullptr:malloc(size);}
struct FakeQueue {unsigned capacity=0,size=0;std::deque<std::vector<uint8_t>> items;};
using QueueHandle_t=FakeQueue*;struct StaticQueue_t{};constexpr int pdTRUE=1;
QueueHandle_t xQueueCreateStatic(unsigned capacity,unsigned size,uint8_t*,StaticQueue_t*){return gFailQueue?nullptr:new FakeQueue{capacity,size,{}};}
int xQueueSend(QueueHandle_t queue,const void* value,int){if(queue->items.size()>=queue->capacity)return 0;queue->items.emplace_back((const uint8_t*)value,(const uint8_t*)value+queue->size);return pdTRUE;}
int xQueueReceive(QueueHandle_t queue,void* value,int){if(queue->items.empty())return 0;memcpy(value,queue->items.front().data(),queue->size);queue->items.pop_front();return pdTRUE;}
namespace dragon::toolkit {
 constexpr unsigned ApCapacity=2;
 struct Ap {bool used=false;uint8_t mac[6]{},channel=6;uint32_t seen=0;};
 Ap gAps[ApCapacity];Ap* aps=gAps;
 struct Target {bool hasAp=true;uint8_t bssid[6]{},channel=6;} target;
 struct Status {bool recording=false;} status;
}
bool radioEnabled=true,gStandby=false,gReconBusy=false,gStorageBusy=false,gBleBusy=false;
uint8_t channelLock=9;
bool csiActive();bool csiStart();void csiStop();void csiLoop();bool csiCommand(const String&);
bool radioStandby(){return gStandby;}
bool reconOwnsRadio(){return gReconBusy||csiActive();}
bool reconStorageBusy(){return gStorageBusy;}
bool bleLabOwnsRadio(){return gBleBusy;}
wifi_mode_t gMode=WIFI_MODE_NULL;wifi_band_mode_t gBand=WIFI_BAND_MODE_AUTO;
wifi_bandwidths_t gBandwidth{WIFI_BW_HT40,WIFI_BW_HT20};
wifi_ps_type_t gPower=WIFI_PS_MIN_MODEM;uint8_t gChannel=11;wifi_second_chan_t gSecond=0;
bool gPromiscuous=true,gCsi=false;wifi_csi_config_t gConfig{};wifi_csi_cb_t gCallback=nullptr;
unsigned gBlockedRadioCalls=0;
void setRadio(bool enabled){if(reconOwnsRadio()){++gBlockedRadioCalls;return;}radioEnabled=enabled;gPromiscuous=enabled;}
std::map<std::string,unsigned> gCalls;
std::string gFailOperation;unsigned gFailOrdinal=1;
bool failed(const char* operation){unsigned nth=++gCalls[operation];return gFailOperation==operation&&nth==gFailOrdinal;}
void failure(const char* operation="",unsigned nth=1){gCalls.clear();gFailOperation=operation;gFailOrdinal=nth;}
#define GETTER(name,type,value) int name(type* out){if(failed(#name))return ESP_FAIL;*out=value;return ESP_OK;}
GETTER(esp_wifi_get_mode,wifi_mode_t,gMode)
GETTER(esp_wifi_get_band_mode,wifi_band_mode_t,gBand)
GETTER(esp_wifi_get_ps,wifi_ps_type_t,gPower)
GETTER(esp_wifi_get_promiscuous,bool,gPromiscuous)
GETTER(esp_wifi_get_csi_config,wifi_csi_config_t,gConfig)
#undef GETTER
int esp_wifi_get_bandwidths(int,wifi_bandwidths_t* out){if(failed("esp_wifi_get_bandwidths"))return ESP_FAIL;if(gBand!=WIFI_BAND_MODE_5G_ONLY)out->ghz_2g=gBandwidth.ghz_2g;if(gBand!=WIFI_BAND_MODE_2G_ONLY)out->ghz_5g=gBandwidth.ghz_5g;return ESP_OK;}
int esp_wifi_get_channel(uint8_t* channel,wifi_second_chan_t* second){if(failed("esp_wifi_get_channel"))return ESP_FAIL;*channel=gChannel;*second=gSecond;return ESP_OK;}
#define SETTER(name,type,value) int name(type input){if(failed(#name))return ESP_FAIL;value=input;return ESP_OK;}
SETTER(esp_wifi_set_mode,wifi_mode_t,gMode)
SETTER(esp_wifi_set_band_mode,wifi_band_mode_t,gBand)
SETTER(esp_wifi_set_ps,wifi_ps_type_t,gPower)
SETTER(esp_wifi_set_promiscuous,bool,gPromiscuous)
SETTER(esp_wifi_set_csi,bool,gCsi)
#undef SETTER
int esp_wifi_set_bandwidths(int,wifi_bandwidths_t* input){if(failed("esp_wifi_set_bandwidths"))return ESP_FAIL;if(gBand!=WIFI_BAND_MODE_5G_ONLY)gBandwidth.ghz_2g=input->ghz_2g;if(gBand!=WIFI_BAND_MODE_2G_ONLY)gBandwidth.ghz_5g=input->ghz_5g;return ESP_OK;}
int esp_wifi_set_channel(uint8_t channel,wifi_second_chan_t second){if(failed("esp_wifi_set_channel"))return ESP_FAIL;gChannel=channel;gSecond=second;return ESP_OK;}
int esp_wifi_set_csi_config(const wifi_csi_config_t* config){if(failed("esp_wifi_set_csi_config"))return ESP_FAIL;gConfig=*config;return ESP_OK;}
int esp_wifi_set_csi_rx_cb(wifi_csi_cb_t callback,void*){if(failed("esp_wifi_set_csi_rx_cb"))return ESP_FAIL;gCallback=callback;return ESP_OK;}
