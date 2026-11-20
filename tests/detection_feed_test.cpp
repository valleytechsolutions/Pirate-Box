#include "detection_feed.h"
#include <cassert>
#include <cstdio>
using namespace dragon;
int main(){
 piratebox::DetectionFeed<3> feed;DeviceKey a{{1,2,3,4,5,6},1,0},b=a,c=a,d=a;b.mac[5]=7;c.mac[5]=8;d.mac[5]=9;
 auto add=[&](DeviceKey key,uint32_t at){return feed.observe(key,Family::Tracker,Confidence::Low,-65,1,"test",0x123456,at);};
 assert(add(a,100));assert(add(b,200));assert(!add(a,300));assert(feed.count==2&&feed.entries[0].key==a&&feed.entries[1].key==b);
 assert(feed.entries[0].observations==2&&feed.coalesced==1);assert(!add(a,300)&&feed.entries[0].observations==2);assert(!add(a,299)&&feed.entries[0].lastAt==300);
 assert(add(c,400));assert(add(d,500));assert(feed.count==3&&feed.entries[0].key==b&&feed.evicted==1);
 assert(add(b,30200));assert(feed.entries[2].key==b&&feed.entries[2].observations==1);
 feed.clear();assert(add(a,UINT32_MAX-10));assert(!add(a,20));assert(feed.entries[0].observations==2);feed.entries[0].observations=65535;assert(!add(a,30));assert(feed.entries[0].observations==65535);
 char text[40];piratebox::feedIdentity(feed.entries[0],true,text,sizeof(text));assert(!strcmp(text,"private device"));strcpy(feed.entries[0].name,"name\nwith\tcontrols");piratebox::feedIdentity(feed.entries[0],false,text,sizeof(text));assert(!strcmp(text,"name with controls"));feed.entries[0].name[0]=0;piratebox::feedIdentity(feed.entries[0],false,text,sizeof(text));assert(!strcmp(text,"01:02:03:04:05:06"));
 DetectorSettings settings;settings.minimum=Confidence::Strong;Contact contact;contact.used=true;contact.packets=1;contact.seen=contact.matchedAt=100;contact.matchHits=1;
 for(unsigned f=0;f<unsigned(Family::Count);++f){contact.match={Family(f),Confidence::Low,"test"};assert(piratebox::feedObservationAllowed(contact,100,settings,false,false,false,false,true));}
 assert(!piratebox::feedObservationAllowed(contact,100,settings,false,true,false,false,true));assert(!piratebox::feedObservationAllowed(contact,100,settings,false,false,true,true,true));assert(!piratebox::feedObservationAllowed(contact,100,settings,false,false,false,true,false));
 contact.match={Family::Wifi,Confidence::None,""};contact.matchHits=0;assert(!piratebox::feedObservationAllowed(contact,100,settings,false,false,false,false,true));assert(piratebox::feedObservationAllowed(contact,100,settings,true,false,false,false,true));assert(piratebox::feedObservationAllowed(contact,100,settings,false,false,false,true,true));
 settings.families=0;assert(!piratebox::feedObservationAllowed(contact,100,settings,true,false,false,false,true));assert(piratebox::feedObservationAllowed(contact,100,settings,false,false,false,true,true));
 auto entry=feed.entries[0];assert(!piratebox::feedEntryAllowed(entry,settings,true,false,false,false,true));assert(piratebox::feedEntryAllowed(entry,settings,false,false,false,true,true));assert(!piratebox::feedEntryAllowed(entry,settings,true,true,false,true,true));
 for(int radius=0;radius<=220;++radius)for(int degree=0;degree<360;++degree){float angle=degree*3.14159265f/180;auto p=piratebox::piratePlotPoint(radius,cosf(angle),sinf(angle));assert(p.x>=13&&p.x<=307&&p.y-8>=70&&p.y+8<piratebox::FeedTop);}
 for(unsigned i=0;i<4;++i){auto box=piratebox::pirateLabelBox(i);assert(box.x>=0&&box.x+box.w<=320&&box.y>=70&&box.y+box.h<piratebox::FeedTop);}
 puts("PASS bounded feed ordering, coalescing, identity privacy, all-family visibility and square plot/feed geometry");
}
