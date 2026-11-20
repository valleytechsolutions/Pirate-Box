#define BOARD_WS_C5_273 1
#include "csi_c5_host_stub.h"
#include "../firmware/src/csi_monitor_c5.cpp"

void initial(){
 failure();if(csiActive())csiStop();assert(!csiActive());
 active=false;owned=false;gMode=WIFI_MODE_NULL;gBand=WIFI_BAND_MODE_AUTO;
 gBandwidth={WIFI_BW_HT40,WIFI_BW_HT20};gPower=WIFI_PS_MIN_MODEM;gChannel=11;gSecond=0;
 gPromiscuous=true;gCsi=false;gCallback=nullptr;gConfig={};gConfig.val_scale_cfg=3;
 gNow=50000;radioEnabled=true;channelLock=9;gBlockedRadioCalls=0;
 gStandby=gReconBusy=gStorageBusy=gBleBusy=false;
 dragon::toolkit::status.recording=false;dragon::toolkit::target={};
 dragon::toolkit::target.hasAp=true;dragon::toolkit::target.channel=6;
 auto& ap=dragon::toolkit::gAps[0];ap={};ap.used=true;ap.channel=6;ap.seen=gNow;
 failure();Serial.text.clear();
}
void restored(){
 assert(!csiActive()&&!gCsi&&!gCallback&&radioEnabled&&channelLock==9);
 assert(gMode==WIFI_MODE_NULL&&gBand==WIFI_BAND_MODE_AUTO&&gBandwidth.ghz_2g==WIFI_BW_HT40&&gBandwidth.ghz_5g==WIFI_BW_HT20);
 assert(gPower==WIFI_PS_MIN_MODEM&&gChannel==11&&gSecond==0&&gPromiscuous&&gConfig.val_scale_cfg==3);
 assert(!gBlockedRadioCalls);
}
int8_t gIq[106];
wifi_csi_info_t sample(){wifi_csi_info_t info;memset(gIq,10,sizeof(gIq));info.buf=gIq;info.len=sizeof(gIq);return info;}
void deliver(wifi_csi_info_t info){assert(gCallback);gCallback(nullptr,&info);}
void fill(unsigned n,int rssi=-45){auto info=sample();info.rx_ctrl.rssi=rssi;for(unsigned i=0;i<n;++i)deliver(info);}

void analyzerTests(){
 // Existing S3 acceptance, scaling invariance, zero-energy and baseline cases.
 dragon::csi::Shape s3;int8_t iq[128]{};
 for(unsigned i=4;i<128;++i)iq[i]=10;
 assert(s3.observe(iq,127)<0&&s3.observe(iq,106)<0&&s3.observe(iq,128)<0);
 assert(s3.observe(iq,128)<.00001f);
 for(unsigned i=4;i<128;++i)iq[i]=20;
 assert(s3.observe(iq,128)<.00001f);
 for(unsigned i=4;i<64;++i)iq[i]=0;
 assert(s3.observe(iq,128)>.1f);
 dragon::csi::Baseline b;for(unsigned i=0;i<30;++i)b.add(.001f);
 assert(b.score(.001f)==0&&b.score(.5f)==100&&b.threshold()>=.005f);b.add(NAN);assert(b.count==30);
 memset(iq,0,sizeof(iq));assert(s3.observe(iq,128)<0);
 dragon::csi::ShapeFor<53> c5;auto in=sample();
 assert(c5.observe(in.buf,105)<0&&c5.observe(in.buf,107)<0&&c5.observe(in.buf,128)<0&&c5.observe(nullptr,106)<0);
 assert(c5.observe(in.buf,106)<0&&c5.observe(in.buf,106)<.00001f);
 // The invalid first word never influences either layout's normalized shape.
 memset(in.buf,-127,4);assert(c5.observe(in.buf,106)<.00001f);
 for(unsigned i=4;i<106;++i)in.buf[i]*=2;assert(c5.observe(in.buf,106)<.00001f);
 for(unsigned i=4;i<54;++i)in.buf[i]=0;assert(c5.observe(in.buf,106)>.1f);
 memset(in.buf,0,106);assert(c5.observe(in.buf,106)<0);
}
void gates(){
 initial();gFailAlloc=true;assert(!csiStart()&&!csiActive()&&radioEnabled);gFailAlloc=false;
 gFailQueue=true;assert(!csiStart()&&!csiActive()&&radioEnabled);gFailQueue=false;
 for(bool* busy:{&gStandby,&gReconBusy,&gStorageBusy,&gBleBusy,&dragon::toolkit::status.recording}){
   initial();*busy=true;assert(!csiStart()&&!csiActive()&&radioEnabled);
 }
 initial();dragon::toolkit::target.hasAp=false;assert(!csiStart());
 for(unsigned channel:{0u,12u,36u}){initial();dragon::toolkit::target.channel=channel;assert(!csiStart());}
 initial();dragon::toolkit::gAps[0].seen=gNow-30000;assert(!csiStart());
 initial();dragon::toolkit::gAps[0].mac[0]=1;assert(!csiStart());
 initial();gBand=WIFI_BAND_MODE_5G_ONLY;assert(!csiStart()&&!csiActive()&&radioEnabled&&gBand==WIFI_BAND_MODE_5G_ONLY);
 for(auto& entry:gCalls)assert(entry.first.rfind("esp_wifi_set_",0)!=0);
}
void callbackTests(){
 initial();assert(csiStart());assert(active&&owned&&!radioEnabled&&gCsi&&gCallback);
 assert(gMode==WIFI_MODE_STA&&gBand==WIFI_BAND_MODE_2G_ONLY&&gPower==WIFI_PS_NONE&&gChannel==6&&gSecond==0);
 assert(gBandwidth.ghz_2g==WIFI_BW_HT20&&gConfig.enable&&gConfig.acquire_csi_legacy&&gConfig.acquire_csi_force_lltf&&gConfig.acquire_csi_ht20&&gConfig.lltf_bit_mode==1);
 assert(!gConfig.acquire_csi_ht40&&!gConfig.acquire_csi_vht&&!gConfig.acquire_csi_su&&!gConfig.acquire_csi_mu&&!gConfig.dump_ack_en);
 assert(!csiStart());
 auto good=sample();gCallback(nullptr,nullptr);auto bad=good;bad.buf=nullptr;deliver(bad);assert(callbacks==0);
 bad=good;bad.mac[0]=1;deliver(bad);bad=good;bad.rx_ctrl.channel=11;deliver(bad);assert(callbacks==2&&sourcePackets==0);
 for(unsigned len:{0u,104u,105u,107u,128u,212u,490u}){bad=good;bad.len=len;deliver(bad);}
 bad=good;bad.rx_ctrl.rx_state=1;deliver(bad);
 bad=good;bad.rx_ctrl.rxend_state=1;deliver(bad);
 bad=good;bad.rx_ctrl.rx_channel_estimate_info_vld=0;deliver(bad);
 bad=good;bad.rx_ctrl.second=1;deliver(bad);
 bad=good;bad.rx_ctrl.cur_bb_format=0;deliver(bad);
 bad=good;bad.rx_ctrl.cur_bb_format=RX_BB_FORMAT_HE_SU;deliver(bad);
 assert(queue->items.empty()&&rejected==13&&layout==-1);
 deliver(good);assert(queue->items.size()==1&&layout==int(RX_BB_FORMAT_11G));
 bad=good;bad.rx_ctrl.cur_bb_format=RX_BB_FORMAT_HT;deliver(bad);assert(queue->items.size()==1&&rejected==14);
 good.first_word_invalid=true;memset(gIq,-128,4);deliver(good);csiLoop();assert(total==1&&variation==0);
 fill(33);assert(queue->items.size()==32&&dropped==1);csiLoop();assert(queue->items.empty());
 auto staleCallback=gCallback;csiStop();restored();unsigned before=callbacks;
 staleCallback(nullptr,&good);assert(callbacks==before);
 // HT LLTF is accepted when it establishes the session's fixed layout.
 initial();assert(csiStart());good=sample();good.rx_ctrl.cur_bb_format=RX_BB_FORMAT_HT;deliver(good);
 assert(layout==int(RX_BB_FORMAT_HT)&&queue->items.size()==1);csiStop();restored();
}
void calibrationTests(){
 initial();assert(csiStart());
 for(unsigned sec=1;sec<=31;++sec){gNow+=1000;fill(10);csiLoop();}
 assert(baseline.count>=20&&!strcmp(state,"STABLE")&&score==0&&quality==100&&total>290);
 gNow+=1000;fill(5,-30);fill(5,-80);csiLoop();assert(!strcmp(state,"LOW DATA")&&score==0&&quality==0);
 gNow+=1000;fill(10);csiLoop();assert(!strcmp(state,"STABLE"));
 fill(2);assert(csiCommand("csi calibrate")&&queue->items.empty()&&baseline.count==0&&!strcmp(state,"CALIBRATING"));
 assert(csiCommand("csi status")&&Serial.text.find("format=c5-lltf8-106")!=std::string::npos);
 assert(csiCommand("csi history")&&!csiCommand("other command"));
 gNow=started+900000;csiLoop();restored();
 initial();gNow=0xfffffff0;dragon::toolkit::gAps[0].seen=gNow;assert(csiStart());
 gNow+=1000;fill(10);csiLoop();assert(csiActive()&&total==9&&baseline.count==1);csiStop();restored();
 initial();radioEnabled=false;gPromiscuous=false;assert(csiStart());csiStop();assert(!radioEnabled&&!gPromiscuous&&!csiActive());
 initial();gBand=WIFI_BAND_MODE_2G_ONLY;gBandwidth.ghz_5g=WIFI_BW_HT40;assert(csiStart());csiStop();
 assert(!csiActive()&&gBand==WIFI_BAND_MODE_2G_ONLY&&gBandwidth.ghz_2g==WIFI_BW_HT40&&gBandwidth.ghz_5g==WIFI_BW_HT40);
}
void failureTests(){
 const char* getters[]={"esp_wifi_get_mode","esp_wifi_get_band_mode","esp_wifi_get_bandwidths","esp_wifi_get_ps","esp_wifi_get_channel","esp_wifi_get_promiscuous","esp_wifi_get_csi_config"};
 for(const char* operation:getters){initial();failure(operation);assert(!csiStart());restored();}
 const char* setters[]={"esp_wifi_set_mode","esp_wifi_set_band_mode","esp_wifi_set_bandwidths","esp_wifi_set_ps","esp_wifi_set_channel","esp_wifi_set_csi_config","esp_wifi_set_csi_rx_cb","esp_wifi_set_promiscuous","esp_wifi_set_csi"};
 for(const char* operation:setters){initial();failure(operation);assert(!csiStart());restored();}
 // Each failed restoration holds ownership, never resumes radar, and can retry.
 for(const char* operation:setters){
   initial();assert(csiStart());failure(operation);csiStop();
   assert(csiActive()&&!active&&!radioEnabled&&!strcmp(state,"RESTORE ERROR"));
   assert(!csiStart());failure();csiStop();restored();
 }
 initial();assert(csiStart());failure("esp_wifi_set_promiscuous",2);csiStop();
 assert(csiActive()&&!active&&!radioEnabled);failure();csiStop();restored();
}
int main(){
 analyzerTests();gates();callbackTests();calibrationTests();failureTests();
 puts("PASS production C5 CSI: fixed106-byte8-bitLLTF, strict metadata/layout gates, queue bounds, calibration/low-data, timeout/rollover, original S3 analyzer and every setup/restore API fault");
}
