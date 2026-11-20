// Passive CSI adapter for the public ESP-IDF 5.5.5 ESP32-C5 API.
// No private driver symbols, transmit hooks, packet generation or association.
#include "csi_monitor.h"
#if defined(BOARD_WS_C5_273) && !defined(BOARD_RADAR_LITE)
#include "app.h"
#include "ble_lab.h"
#include "recon_internal.h"
#include "csi_analysis.h"
#include <esp_wifi.h>
#include <esp_idf_version.h>
#include <atomic>
#if ESP_IDF_VERSION != ESP_IDF_VERSION_VAL(5,5,5)
#error Revalidate the C5 LLTF representation before changing the pinned IDF
#endif
namespace {
constexpr size_t CsiBytes=106;
std::atomic<bool> active{false},owned{false};
std::atomic<uint32_t> dropped{0},callbacks{0},sourcePackets{0},rejected{0};
std::atomic<unsigned> lastLength{0},lastMode{0};
std::atomic<int> layout{-1};
bool resume=false;
uint8_t oldLock=0,channel=0,source[6]{};
struct RadioState {
 wifi_mode_t mode=WIFI_MODE_NULL;
 wifi_band_mode_t band=WIFI_BAND_MODE_2G_ONLY;
 wifi_bandwidths_t bandwidth{};
 wifi_ps_type_t power=WIFI_PS_NONE;
 uint8_t channel=1;wifi_second_chan_t second=WIFI_SECOND_CHAN_NONE;
 bool promiscuous=false;wifi_csi_config_t csi{};
} previous;
struct Sample {int8_t iq[CsiBytes],rssi;uint32_t at;};
QueueHandle_t queue=nullptr;StaticQueue_t control;uint8_t* memory=nullptr;
dragon::csi::ShapeFor<53> shape;
dragon::csi::Baseline baseline;
uint32_t started=0,calibration=0,window=0,lastSample=0,total=0,previousDrops=0;
unsigned windowCount=0,cadence=0,score=0,quality=0;
float sum=0,rssiSum=0,rssiSq=0,variation=0;
const char* state="STOPPED";char message[128]="Select a fresh 2.4 GHz AP, then Start";
struct Point {uint32_t at;uint16_t rate;uint8_t score,quality;float variation;};
Point history[120]{};unsigned count=0,head=0;

void received(void*,wifi_csi_info_t* info){
 if(!active.load()||!info||!info->buf)return;
 ++callbacks;
 if(info->rx_ctrl.channel!=channel||memcmp(info->mac,source,6))return;
 ++sourcePackets;lastLength=info->len;lastMode=info->rx_ctrl.cur_bb_format;
 // IDF 5.5.5: forced 8-bit LLTF contains 53 complex bins, exactly 106
 // bytes. Other PHY layouts/widths must never enter the same baseline.
 const unsigned format=info->rx_ctrl.cur_bb_format;
 if(info->rx_ctrl.rx_state||info->rx_ctrl.rxend_state||
    !info->rx_ctrl.rx_channel_estimate_info_vld||info->rx_ctrl.second!=0||
    (format!=RX_BB_FORMAT_11G&&format!=RX_BB_FORMAT_HT)||info->len!=CsiBytes){
   ++rejected;return;
 }
 int unset=-1;layout.compare_exchange_strong(unset,int(format));
 if(layout.load()!=int(format)){++rejected;return;}
 // ShapeFor always omits the first two IQ pairs. This also discards the
 // invalid first word when first_word_invalid is set without mixing shapes.
 Sample sample{};memcpy(sample.iq,info->buf,CsiBytes);
 sample.rssi=info->rx_ctrl.rssi;sample.at=millis();
 if(xQueueSend(queue,&sample,0)!=pdTRUE)++dropped;
}
void recalibrate(){
 shape={};baseline={};calibration=window=millis();lastSample=0;
 windowCount=0;sum=rssiSum=rssiSq=variation=0;score=quality=cadence=0;
 previousDrops=dropped;count=head=0;state="CALIBRATING";
}
esp_err_t saveRadio(){
 // This module is the firmware's sole CSI callback owner on C5; the S3
 // implementation is excluded. Public IDF has no callback/enabled getter,
 // so the known initial CSI state is disabled and restoration keeps it off.
 esp_err_t err=esp_wifi_get_mode(&previous.mode);
 if(err==ESP_OK)err=esp_wifi_get_band_mode(&previous.band);
 // get_bandwidths only reports enabled bands. Do not alter an unobservable
 // 2.4 GHz configuration when entering from a 5 GHz-only owner.
 if(err==ESP_OK&&previous.band!=WIFI_BAND_MODE_2G_ONLY&&previous.band!=WIFI_BAND_MODE_AUTO)return ESP_ERR_NOT_SUPPORTED;
 if(err==ESP_OK)err=esp_wifi_get_bandwidths(WIFI_IF_STA,&previous.bandwidth);
 if(err==ESP_OK)err=esp_wifi_get_ps(&previous.power);
 if(err==ESP_OK)err=esp_wifi_get_channel(&previous.channel,&previous.second);
 if(err==ESP_OK)err=esp_wifi_get_promiscuous(&previous.promiscuous);
 if(err==ESP_OK)err=esp_wifi_get_csi_config(&previous.csi);
 return err;
}
esp_err_t restoreRadio(){
 // Try every operation even if one fails. Hold ownership and leave radar
 // paused on any failure; STOP can retry restoration without losing state.
 esp_err_t first=ESP_OK;
 auto keep=[&](esp_err_t result){if(first==ESP_OK&&result!=ESP_OK)first=result;};
 keep(esp_wifi_set_csi(false));
 keep(esp_wifi_set_csi_rx_cb(nullptr,nullptr));
 keep(esp_wifi_set_promiscuous(false));
 keep(esp_wifi_set_csi_config(&previous.csi));
 keep(esp_wifi_set_band_mode(previous.band));
 keep(esp_wifi_set_bandwidths(WIFI_IF_STA,&previous.bandwidth));
 keep(esp_wifi_set_ps(previous.power));
 keep(esp_wifi_set_channel(previous.channel,previous.second));
 keep(esp_wifi_set_mode(previous.mode));
 if(first==ESP_OK)keep(esp_wifi_set_promiscuous(previous.promiscuous));
 return first;
}
}
bool csiActive(){return active.load()||owned;}
void csiStop(){
 active=false;
 if(!owned)return;
 esp_err_t err=restoreRadio();
 if(err!=ESP_OK){state="RESTORE ERROR";snprintf(message,sizeof(message),"Radio restore: %s / press STOP to retry",esp_err_to_name(err));return;}
 channelLock=oldLock;owned=false;setRadio(resume);state="STOPPED";
 strlcpy(message,"CSI stopped / radar restored",sizeof(message));
}
bool csiStart(){using namespace dragon::toolkit;
 if(csiActive()||radioStandby()||reconOwnsRadio()||reconStorageBusy()||bleLabOwnsRadio()||status.recording||!target.hasAp){
   strlcpy(message,"Leave clock/timer, stop other work and select a fresh AP",sizeof(message));return false;
 }
 // The shared survey/channel scheduler currently covers US channels 1..11.
 // C5 5 GHz CSI is not claimed by this bounded S3-feature parity adapter.
 if(target.channel<1||target.channel>11){strlcpy(message,"Select a fresh 2.4 GHz AP on channels 1-11",sizeof(message));return false;}
 bool fresh=false;
 for(unsigned i=0;aps&&i<ApCapacity;++i)
   if(aps[i].used&&!memcmp(aps[i].mac,target.bssid,6)&&millis()-aps[i].seen<30000&&aps[i].channel==target.channel)fresh=true;
 if(!fresh){strlcpy(message,"Selected AP is stale; survey and select it again",sizeof(message));return false;}
 if(!queue){
   if(!memory)memory=(uint8_t*)ps_malloc(32*sizeof(Sample));
   if(!memory){strlcpy(message,"CSI queue memory unavailable",sizeof(message));return false;}
   queue=xQueueCreateStatic(32,sizeof(Sample),memory,&control);
   if(!queue){strlcpy(message,"CSI queue unavailable",sizeof(message));return false;}
 }
 Sample stale;while(xQueueReceive(queue,&stale,0)==pdTRUE){}
 esp_err_t err=saveRadio();
 if(err!=ESP_OK){snprintf(message,sizeof(message),"Cannot save radio state: %s",esp_err_to_name(err));return false;}
 resume=radioEnabled;oldLock=channelLock;memcpy(source,target.bssid,6);channel=target.channel;
 setRadio(false);owned=true;
 dropped=callbacks=sourcePackets=rejected=0;lastLength=lastMode=0;layout=-1;
 total=0;started=millis();recalibrate();
 wifi_csi_config_t config{};
 config.enable=true;config.acquire_csi_legacy=true;
 config.acquire_csi_force_lltf=true;config.acquire_csi_ht20=true;
 config.lltf_bit_mode=1; // Explicit 8-bit components; default is 12-bit.
 // HT40/VHT/HE/ACK capture stays disabled. No active traffic is generated.
 wifi_bandwidths_t bandwidth{};bandwidth.ghz_2g=WIFI_BW_HT20;bandwidth.ghz_5g=WIFI_BW_HT20;
 err=esp_wifi_set_mode(WIFI_MODE_STA);
 if(err==ESP_OK)err=esp_wifi_set_band_mode(WIFI_BAND_MODE_2G_ONLY);
 if(err==ESP_OK)err=esp_wifi_set_bandwidths(WIFI_IF_STA,&bandwidth);
 if(err==ESP_OK)err=esp_wifi_set_ps(WIFI_PS_NONE);
 if(err==ESP_OK)err=esp_wifi_set_channel(channel,WIFI_SECOND_CHAN_NONE);
 if(err==ESP_OK)err=esp_wifi_set_csi_config(&config);
 if(err==ESP_OK)err=esp_wifi_set_csi_rx_cb(received,nullptr);
 if(err==ESP_OK)err=esp_wifi_set_promiscuous(true);
 if(err==ESP_OK)err=esp_wifi_set_csi(true);
 if(err!=ESP_OK){
   csiStop();
   if(!owned)snprintf(message,sizeof(message),"CSI setup: %s",esp_err_to_name(err));
   return false;
 }
 active=true;strlcpy(message,"Keep AP and detector still during calibration",sizeof(message));return true;
}
void csiLoop(){
 if(!active.load())return;
 uint32_t now=millis();if(now-started>=900000){csiStop();return;}
 Sample sample;
 for(unsigned i=0;i<32&&xQueueReceive(queue,&sample,0)==pdTRUE;++i){
   float value=shape.observe(sample.iq,sizeof(sample.iq));if(value<0)continue;
   lastSample=sample.at;++total;++windowCount;sum+=value;
   rssiSum+=sample.rssi;rssiSq+=float(sample.rssi)*sample.rssi;
 }
 if(now-window<1000)return;
 cadence=windowCount*1000u/(now-window);uint32_t losses=dropped.load()-previousDrops;previousDrops=dropped;
 variation=windowCount?sum/windowCount:0;float mean=windowCount?rssiSum/windowCount:0;
 float deviation=std::sqrt(std::max(0.0f,windowCount?rssiSq/windowCount-mean*mean:0));
 bool usable=cadence>=5&&now-lastSample<1500&&losses<=2&&deviation<6;
 quality=usable?unsigned(std::max(0.0f,100-deviation*8-losses*10)):0;
 if(now-calibration<30000){state="CALIBRATING";if(usable)baseline.add(variation);score=0;}
 else if(!usable||baseline.count<20){state="LOW DATA";score=0;}
 else {score=baseline.score(variation);state=variation>baseline.threshold()?"CHANGE":"STABLE";}
 history[head]={now,uint16_t(std::min(cadence,65535u)),uint8_t(score),uint8_t(quality),variation};
 head=(head+1)%120;count=std::min(count+1,120u);
 window=now;windowCount=0;sum=rssiSum=rssiSq=0;
}
void csiStatus(char* out,size_t capacity){
 snprintf(out,capacity,"%s / CH %u\nChange score %u / 100\nSignal quality %u%% / %u samples/s\nSamples %lu / queue drops %lu\nCalibration %u / 30 seconds\n\n%s\n\nRadio-path changes, not a person or occupancy detector.",state,channel,score,quality,cadence,static_cast<unsigned long>(total),static_cast<unsigned long>(dropped.load()),calibration?std::min(30u,unsigned((millis()-calibration)/1000)):0,message);
}
bool csiCommand(const String& command){
 if(!command.startsWith("csi "))return false;
 if(command=="csi start")Serial.println(csiStart()?"RECON_OK CSI started":"RECON_ERROR fresh AP / busy / CSI setup");
 else if(command=="csi stop"){csiStop();Serial.println(owned?"RECON_ERROR CSI radio restoration failed":"RECON_OK");}
 else if(command=="csi calibrate"&&active.load()){
   Sample stale;while(xQueueReceive(queue,&stale,0)==pdTRUE){}recalibrate();Serial.println("RECON_OK calibrating");
 }
 else if(command=="csi status"){
   Serial.printf("RECON_CSI active=%u state=%s channel=%u score=%u quality=%u rate=%u samples=%lu drops=%lu callbacks=%lu source_packets=%lu rejected=%lu last_length=%u last_mode=%u layout=%d baseline=%u variation=%.6f threshold=%.6f format=c5-lltf8-106 restore_pending=%u message=%s\nRECON_OK\n",active.load(),state,channel,score,quality,cadence,static_cast<unsigned long>(total),static_cast<unsigned long>(dropped.load()),static_cast<unsigned long>(callbacks.load()),static_cast<unsigned long>(sourcePackets.load()),static_cast<unsigned long>(rejected.load()),lastLength.load(),lastMode.load(),layout.load(),baseline.count,variation,baseline.threshold(),owned.load()&&!active.load(),message);
 }
 else if(command=="csi history"){
   Serial.println("RECON_CSI_CSV uptime_ms,samples_per_second,change_score,signal_quality,variation");
   for(unsigned i=0;i<count;++i){auto& point=history[(head+120-count+i)%120];Serial.printf("RECON_CSI_POINT %lu,%u,%u,%u,%.6f\n",static_cast<unsigned long>(point.at),point.rate,point.score,point.quality,point.variation);}Serial.println("RECON_OK");
 }
 else Serial.println("RECON_ERROR CSI command or inactive");
 return true;
}
#endif
