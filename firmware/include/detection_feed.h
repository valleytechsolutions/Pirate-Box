#pragma once
#include "contacts.h"
#include <cstdio>
#include <cstring>
#include <limits>

namespace piratebox {
constexpr uint32_t FeedEncounterGapMs=30000;
struct FeedEntry {
 dragon::DeviceKey key;
 dragon::Family family=dragon::Family::Wifi;
 dragon::Confidence confidence=dragon::Confidence::None;
 uint32_t firstAt=0,lastAt=0,color=0;
 uint16_t observations=0;
 int16_t rssi=-127;
 uint8_t channel=0;
 char name[40]{};
};
// Same visibility policy as radar; snoozed entries additionally stay out of
// the live event feed. Alert confidence/throttling never hides visible tags.
inline bool feedObservationAllowed(const Contact& contact,uint32_t now,
 const dragon::DetectorSettings& settings,bool all,bool ignored,bool snoozed,bool watched,bool modeAccepted){
 return modeAccepted&&!snoozed&&candidateVisible(contact,now,settings,all,ignored,watched);
}
inline bool feedEntryAllowed(const FeedEntry& entry,const dragon::DetectorSettings& settings,
 bool all,bool ignored,bool snoozed,bool watched,bool modeAccepted){
 return !ignored&&!snoozed&&modeAccepted&&(watched||(settings.enabled(entry.family)&&
   (all||entry.confidence!=dragon::Confidence::None)));
}
inline void feedIdentity(const FeedEntry& entry,bool privacy,char* out,size_t capacity){
 if(!capacity)return;
 if(privacy){snprintf(out,capacity,"private device");return;}
 if(!entry.name[0]){snprintf(out,capacity,"%02X:%02X:%02X:%02X:%02X:%02X",entry.key.mac[0],entry.key.mac[1],entry.key.mac[2],entry.key.mac[3],entry.key.mac[4],entry.key.mac[5]);return;}
 size_t n=std::min(capacity-1,strlen(entry.name));
 for(size_t i=0;i<n;++i){unsigned char c=entry.name[i];out[i]=c<32||c==127?' ':char(c);}out[n]=0;
}
template<size_t Capacity=32> struct DetectionFeed {
 static_assert(Capacity>0&&Capacity<=64,"Feed history must remain bounded");
 FeedEntry entries[Capacity]{};size_t count=0;
 uint32_t appended=0,coalesced=0,evicted=0;
 void clear(){count=0;appended=coalesced=evicted=0;}
 // Oldest entry is at index zero; the newest encounter is always at the end.
 // Repeated adverts update their existing row without constantly reordering it.
 bool observe(const dragon::DeviceKey& key,dragon::Family family,dragon::Confidence confidence,
              int rssi,uint8_t channel,const char* name,uint32_t color,uint32_t at){
   FeedEntry* row=nullptr;
   for(size_t i=count;i>0;--i){auto& prior=entries[i-1];if(prior.key==key&&prior.family==family){
     if(int32_t(at-prior.lastAt)<0)return false;
     if(uint32_t(at-prior.lastAt)<FeedEncounterGapMs)row=&prior;
     break;
   }}
   bool inserted=row==nullptr;
   if(inserted){
     if(count==Capacity){for(size_t i=1;i<count;++i)entries[i-1]=entries[i];--count;++evicted;}
     row=&entries[count++];*row={};row->key=key;row->family=family;row->firstAt=at;++appended;
   }else if(row->lastAt!=at)++coalesced;
   if(inserted||row->lastAt!=at){if(row->observations<std::numeric_limits<uint16_t>::max())++row->observations;}
   row->lastAt=at;row->confidence=confidence;row->rssi=rssi;row->channel=channel;row->color=color;
   if(name&&*name){size_t n=std::min(strlen(name),sizeof(row->name)-1);memcpy(row->name,name,n);row->name[n]=0;}
   return inserted;
 }
};
// Full square radar surface. Only observations/callouts avoid its integrated
// bottom feed; the animated field and compass sweep fill the entire display.
constexpr int FeedTop=242,FeedHeight=78;
struct PlotPoint {int x,y;};
inline PlotPoint piratePlotPoint(float radius,float unitX,float unitY){
 int x=160+int(lroundf(radius*unitX*.94f)),y=147+int(lroundf(radius*unitY*.53f));
 return {std::max(13,std::min(307,x)),std::max(78,std::min(232,y))};
}
struct LabelBox {int x,y,w=132,h=30;};
constexpr LabelBox pirateLabelBox(unsigned slot){return {12+int(slot%2)*164,78+int(slot/2)*122,132,30};}
}
