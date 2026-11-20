// Native 320px square Pirate Box field. The sweep is decorative, not bearing.
#ifdef BOARD_WS_C5_273
static void pirateRadarDraw(lv_draw_ctx_t* ctx){
 const uint32_t now=radarFrameAt;
 const auto& palette=radarPalette(radarTheme);bool pirate=radarTheme==PirateThemeId;
 uint32_t gold=pirate?0xffc44d:palette.ring,coral=pirate?0xff775a:palette.dot,cyan=pirate?0x28d9ca:palette.sweep;
 uint32_t grid=pirate?0x19738a:palette.grid,panel=pirate?0x061323:palette.panel,muted=pirate?0xb5c8d8:palette.text;
 static uint32_t priorFrame=UINT32_MAX;bool advance=priorFrame!=now;priorFrame=now;
 const int cx=160,cy=147;
 lv_draw_rect_dsc_t bg;lv_draw_rect_dsc_init(&bg);bg.bg_color=color(0x000000);
 lv_area_t screen{0,0,319,319};lv_draw_rect(ctx,&bg,&screen);
 for(int x=0;x<320;x+=32)line(ctx,x,0,x,319,grid,1,70);
 for(int y=0;y<320;y+=32)line(ctx,0,y,319,y,grid,1,70);
 for(int x=12;x<320;x+=20){int n=x%60==12?7:3;line(ctx,x,70,x,70+n,gold,1,135);line(ctx,x,319-n,x,319,cyan,1,100);}
 for(int y=78;y<242;y+=20){line(ctx,0,y,4,y,cyan,1,140);line(ctx,315,y,319,y,coral,1,140);}
 line(ctx,8,70,311,70,grid,1,230);
 line(ctx,8,72,8,104,gold,2);line(ctx,8,72,40,72,gold,2);
 line(ctx,311,72,279,72,coral,2);line(ctx,311,72,311,104,coral,2);
 // A square pulse and a clipped compass sweep occupy the entire display.
 static uint32_t sweepAt=0;if(radioEnabled)sweepAt=now;
 int pulse=int((sweepAt%2800)*176/2800);
 lv_draw_rect_dsc_t ping;lv_draw_rect_dsc_init(&ping);ping.bg_opa=LV_OPA_TRANSP;
 ping.border_color=color(cyan);ping.border_width=1;ping.border_opa=uint8_t(130-pulse*110/176);
 lv_area_t pulseBox{lv_coord_t(cx-pulse),lv_coord_t(cy-pulse),lv_coord_t(cx+pulse),lv_coord_t(cy+pulse)};lv_draw_rect(ctx,&ping,&pulseBox);
 float angle=(sweepAt%5500)*6.28318530718f/5500.f;
 auto edge=[&](float a){float x=cosf(a),y=sinf(a),t=1000;
   if(fabsf(x)>.001f)t=std::min(t,(x>0?319-cx:cx)/fabsf(x));
   if(fabsf(y)>.001f)t=std::min(t,(y>0?319-cy:cy)/fabsf(y));
   return lv_point_t{lv_coord_t(cx+x*t),lv_coord_t(cy+y*t)};
 };
 lv_draw_rect_dsc_t trail;lv_draw_rect_dsc_init(&trail);trail.bg_color=color(cyan);
 for(int n=0;n<9;++n){lv_point_t triangle[]={{cx,cy},edge(angle-n*.055f),edge(angle-(n+1)*.055f)};trail.bg_opa=uint8_t(52-n*5);lv_draw_triangle(ctx,&trail,triangle);}
 auto tip=edge(angle);line(ctx,cx,cy,tip.x,tip.y,cyan,2,195);
 disc(ctx,tip.x,tip.y,3,gold,210);
 line(ctx,cx-11,cy,cx+11,cy,gold,1,180);line(ctx,cx,cy-11,cx,cy+11,gold,1,180);
 disc(ctx,cx,cy,4,gold);disc(ctx,cx,cy,2,coral);

 if(!radarPrepared||uint32_t(now-radarPrepared)>=250){
   radarPrepared=now;radarCount=0;
   for(size_t i=0;i<MAX_CONTACTS;++i)if(radarVisible(contacts[i],now))radarOrder[radarCount++]=i;
   std::sort(radarOrder,radarOrder+radarCount,[now](int a,int b){if(a==trackedContact||b==trackedContact)return a==trackedContact&&b!=trackedContact;return preferContact(contacts[a],contacts[b],now);});
 }
 dotCount=0;
 for(unsigned k=0;k<unsigned(radarCount);++k){int i=radarOrder[k];auto& contact=contacts[i];if(!radarVisible(contact,now))continue;
   float target=contact.signal.count?dragon::radarRadius(contact.signal.smooth):158.f;
   if(!contact.plotRadius)contact.plotRadius=target;else if(advance)contact.plotRadius+=(target-contact.plotRadius)*.16f;
   auto point=piratebox::piratePlotPoint(contact.plotRadius,contact.radarX,contact.radarY);
   dots[dotCount++]={point.x,point.y,i,contactKey(contact)};
 }
 for(int i=dotCount-1;i>=0;--i){auto& dot=dots[i];auto& contact=contacts[dot.index];auto ink=contactColor(contact);
   unsigned age=now-contact.seen;bool tracker=dragon::trackerFamily(visibleFamily(contact,now,detector.families));
   disc(ctx,dot.x,dot.y,tracker?5:8,0x020711);
   if(age>=30000){lv_draw_arc_dsc_t arc;lv_draw_arc_dsc_init(&arc);arc.color=color(ink);arc.width=2;arc.opa=190;lv_point_t center{lv_coord_t(dot.x),lv_coord_t(dot.y)};for(int a=0;a<360;a+=90)lv_draw_arc(ctx,&arc,&center,5,a,a+45);}
   else {disc(ctx,dot.x,dot.y,tracker?3:5,ink);if(!tracker)disc(ctx,dot.x-1,dot.y-1,1,0xffffff);}
 }
 constexpr unsigned LabelCount=4;
 static dragon::RadarLabelCandidate candidates[MAX_CONTACTS];unsigned labels[LabelCount];
 for(int i=0;i<dotCount;++i){auto& c=contacts[dots[i].index];candidates[i]={c.match.family,uint32_t(now-c.seen)>=30000,radarHasSelection&&dots[i].key==radarSelectedKey};}
 unsigned labelCount=dragon::selectRadarLabels(candidates,dotCount,labels,LabelCount,radarLabels);
 radarPrimary=labelCount?dots[labels[0]].index:-1;
 unsigned used=0;
 for(unsigned j=0;j<labelCount;++j){auto& dot=dots[labels[j]];auto& c=contacts[dot.index];int best=0,bestCost=INT32_MAX;
   for(unsigned slot=0;slot<LabelCount;++slot){if(used&(1u<<slot))continue;auto box=piratebox::pirateLabelBox(slot);
     int ex=constrain(dot.x,box.x,box.x+box.w),ey=constrain(dot.y,box.y,box.y+box.h);
     int cost=(dot.x-ex)*(dot.x-ex)+(dot.y-ey)*(dot.y-ey);
     for(int n=0;n<dotCount;++n)if(dots[n].x>=box.x-7&&dots[n].x<=box.x+box.w+7&&dots[n].y>=box.y-7&&dots[n].y<=box.y+box.h+7)cost+=100000;
     if(cost<bestCost){bestCost=cost;best=slot;}
   }
   used|=1u<<best;auto box=piratebox::pirateLabelBox(best);dot.labelSlot=best;
   uint32_t ink=contactColor(c);int ex=constrain(dot.x,box.x,box.x+box.w),ey=constrain(dot.y,box.y,box.y+box.h);
   line(ctx,dot.x,dot.y,ex,ey,0x020711,3);line(ctx,dot.x,dot.y,ex,ey,ink,1,220);
   lv_draw_rect_dsc_t plate;lv_draw_rect_dsc_init(&plate);plate.bg_color=color(panel);plate.bg_opa=245;plate.radius=4;plate.border_color=color(ink);plate.border_width=1;
   lv_area_t area{lv_coord_t(box.x),lv_coord_t(box.y),lv_coord_t(box.x+box.w-1),lv_coord_t(box.y+box.h-1)};lv_draw_rect(ctx,&plate,&area);
   lv_draw_label_dsc_t text;lv_draw_label_dsc_init(&text);text.color=color(ink);text.font=&lv_font_montserrat_12;
   lv_area_t title{lv_coord_t(box.x+5),lv_coord_t(box.y+2),lv_coord_t(box.x+box.w-5),lv_coord_t(box.y+15)};
   lv_area_t clip;const lv_area_t* prior=ctx->clip_area;
   if(_lv_area_intersect(&clip,prior,&area)){ctx->clip_area=&clip;lv_draw_label(ctx,&text,&title,radarFamilyName(c.match.family),nullptr);
     char meta[45];if(uint32_t(now-c.seen)>=10000)snprintf(meta,sizeof(meta),"%lus ago",static_cast<unsigned long>((now-c.seen)/1000));else snprintf(meta,sizeof(meta),"%s  %d dBm",c.ble?"BLE":"Wi-Fi",c.rssi);
     text.font=&lv_font_montserrat_10;text.color=color(muted);title.y1=box.y+16;title.y2=box.y+29;lv_draw_label(ctx,&text,&title,meta,nullptr);ctx->clip_area=prior;
   }
   disc(ctx,dot.x,dot.y,5,ink);
 }
}
#endif
