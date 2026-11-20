#ifdef BOARD_WS_C5_273
static piratebox::DetectionFeed<>* detectionFeed=nullptr;
static lv_obj_t* feedTitle=nullptr;
static lv_obj_t* feedRows[3]{},*feedNames[3]{},*feedMeta[3]{},*feedBars[3]{};
static lv_obj_t *weatherTemperature=nullptr,*weatherFahrenheit=nullptr,*weatherHumidity=nullptr,*weatherClock=nullptr,*weatherHardware=nullptr;
static uint32_t feedRefreshAt=0;
static lv_obj_t *detectionMascot=nullptr,*mascotCaption=nullptr;
static dragon::DeviceKey mascotKey{};
static Family mascotFamily=Family::Count;
static uint32_t mascotEncounterAt=0;
static uint32_t mascotReactionStartedAt=0;
static bool mascotHasEncounter=false;
static void updateDetectionMascot();
static lv_obj_t* squareLabel(lv_obj_t* parent,const char* text,int x,int y,int width,const lv_font_t* font,uint32_t ink){
 auto* object=lv_label_create(parent);lv_label_set_text(object,text);lv_obj_set_pos(object,x,y);lv_obj_set_width(object,width);
 lv_label_set_long_mode(object,LV_LABEL_LONG_DOT);lv_obj_set_style_text_font(object,uiFont(font),0);lv_obj_set_style_text_color(object,color(ink),0);return object;
}
static lv_obj_t* squareButton(lv_obj_t* parent,const char* text,int x,int y,int width,int height,int id,uint32_t accent=0){
 auto* object=lv_btn_create(parent);lv_obj_set_pos(object,x,y);lv_obj_set_size(object,width,height);
 lv_obj_set_style_pad_all(object,0,0);lv_obj_set_style_radius(object,9,0);lv_obj_set_style_shadow_width(object,0,0);
 lv_obj_set_style_bg_color(object,color(uiColors().surface),0);lv_obj_set_style_border_color(object,color(accent?accent:uiColors().border),0);lv_obj_set_style_border_width(object,1,0);
 lv_obj_add_event_cb(object,action,LV_EVENT_CLICKED,(void*)(intptr_t)id);
 auto* caption=squareLabel(object,text,6,0,width-12,&lv_font_montserrat_14,uiColors().ink);
 lv_obj_set_style_text_align(caption,LV_TEXT_ALIGN_CENTER,0);lv_obj_center(caption);return object;
}
static void squareHeading(const char* title,const char* detail=nullptr){
 squareLabel(root,title,12,10,296,&lv_font_montserrat_20,uiColors().ink);
 if(detail)squareLabel(root,detail,12,36,296,&lv_font_montserrat_12,uiColors().muted);
}
static void pirateBrand(){
 auto* image=lv_img_create(root);const auto* title=pirateboxTitleImage();lv_img_set_src(image,title);
 lv_img_set_pivot(image,0,0);lv_img_set_zoom(image,210*256/title->header.w);lv_obj_set_pos(image,10,0);
 lv_obj_clear_flag(image,LV_OBJ_FLAG_CLICKABLE);pirateboxAvatarShow(root,251,5,75);
}
static void buildSquareMenu(){
 if(menuTab<0){
   pirateBrand();squareLabel(root,"YOUR POCKET LOOKOUT",14,77,230,&lv_font_montserrat_10,uiColors().muted);
   const char* names[]={"RADAR","WEATHER","BLE LAB","ATTACKS","TOOLS","SETTINGS"};
   const char* descriptions[]={"Signals + detection feed","Temperature + humidity","GATT + profiles + BLE tools","Wi-Fi + Bluetooth tools","Recon + Wardrive + files","Themes + device controls"};
   const int ids[]={1,190,7000,7103,196,40};const uint32_t colors[]={0x2ce9d1,0x6ecfff,0xc49bff,0xff775a,0x7bda82,0xffc44d};
   for(unsigned i=0;i<6;++i){int x=10+int(i%2)*154,y=102+int(i/2)*58;
     auto* card=squareButton(root,"",x,y,146,54,ids[i],colors[i]);
     lv_obj_set_style_bg_color(card,color(radarTheme==PirateThemeId?0x0a1b2e:uiColors().surface),0);
     lv_obj_set_style_bg_color(card,color(uiColors().raised),LV_STATE_FOCUSED);lv_obj_set_style_border_width(card,2,LV_STATE_FOCUSED);
     auto* title=squareLabel(card,names[i],10,7,126,&lv_font_montserrat_20,colors[i]);lv_obj_set_style_text_font(title,&lv_font_montserrat_20,0);
     auto* detail=squareLabel(card,descriptions[i],10,35,126,&lv_font_montserrat_8,uiColors().muted);lv_obj_set_style_text_font(detail,&lv_font_montserrat_8,0);lv_obj_set_height(detail,11);
   }
   squareLabel(root,"KEY L / R: move     BOOT: select",12,291,296,&lv_font_montserrat_12,uiColors().muted);
   return;
 }
 squareHeading("SHIP'S TOOLS","Choose a mode or open your utilities");
 const char* tabs[]={"HOME","RECON","UTILITIES","SYSTEM"};const int ids[]={197,10,11,12};
 for(int i=0;i<4;++i){auto* tab=squareButton(root,tabs[i],8+i*77,57,73,32,ids[i]);auto* text=lv_obj_get_child(tab,0);lv_obj_set_style_text_font(text,uiFont(&lv_font_montserrat_12),0);if(i==menuTab+1)lv_obj_set_style_bg_color(tab,color(uiColors().raised),0);}
 if(menuTab==0){
   const char* names[]={"Modes","Attacks","Wi-Fi","Bluetooth","History","Hunt","Radio tools","BLE Lab"};
   const int ids[]={7100,7103,20,21,60,57,7102,7000};
   for(unsigned i=0;i<8;++i)squareButton(root,names[i],8+int(i%2)*156,103+int(i/2)*48,148,40,ids[i]);
 }else if(menuTab==1){
   const char* names[]={"Stopwatch","Clock","My badge","SD files","Weather","Activity"};
   const int ids[]={130,133,50,32,190,116};
   for(unsigned i=0;i<6;++i)squareButton(root,names[i],8+int(i%2)*156,103+int(i/2)*65,148,56,ids[i]);
 }else{
   const char* names[]={"Settings","Recon notebook","Radio health","About Pirate Box"};const int ids[]={40,153,99,41};
   for(unsigned i=0;i<4;++i)squareButton(root,names[i],8,103+int(i)*48,304,40,ids[i]);
 }
}
static int squareSettingsSection=-1;
static constexpr int SquareSettingsBack=7200;
static piratebox::SemanticFocus<96,32>* squareSettingsFocus=nullptr;
static bool squareSettingsActive=false,squareSettingsRestore=false;
static Page squareSettingsBuiltPage=Boot;
static int squareSettingsBuiltSection=-1;
static int squareListY=0;
static void squareSettingsRemember(lv_obj_t* object,int id){
 if(!squareSettingsFocus)return;char key[32];
 snprintf(key,sizeof(key),"%u/%d/%d",unsigned(page),squareSettingsSection,id);
 squareSettingsFocus->bind(object,key);
}
static void squareSettingsBeforeBuild(){
 if(!squareSettingsFocus){
   void* memory=heap_caps_malloc(sizeof(*squareSettingsFocus),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
   if(memory)squareSettingsFocus=new(memory) piratebox::SemanticFocus<96,32>();
 }
 if(squareSettingsFocus)squareSettingsFocus->begin(squareSettingsActive&&pending==squareSettingsBuiltPage&&squareSettingsSection==squareSettingsBuiltSection);
 squareSettingsActive=false;squareSettingsRestore=false;
}
static void squareDeckPanel(lv_obj_t* object,uint32_t accent){
 lv_obj_set_style_radius(object,3,0);lv_obj_set_style_shadow_width(object,0,0);
 lv_obj_set_style_pad_all(object,0,0);lv_obj_set_style_bg_color(object,color(uiColors().surface),0);
 lv_obj_set_style_border_color(object,color(accent),0);lv_obj_set_style_border_width(object,1,0);
 lv_obj_set_style_bg_color(object,color(uiColors().raised),LV_STATE_FOCUSED);
 lv_obj_set_style_border_width(object,2,LV_STATE_FOCUSED);
 lv_obj_set_style_outline_color(object,color(uiColors().accent),LV_STATE_FOCUSED);
 lv_obj_set_style_outline_width(object,2,LV_STATE_FOCUSED);lv_obj_set_style_outline_pad(object,0,LV_STATE_FOCUSED);
}
static lv_obj_t* squareDeckButton(lv_obj_t* parent,const char* text,int x,int y,int width,int height,int id,uint32_t accent){
 auto* object=squareButton(parent,text,x,y,width,height,id,accent);squareDeckPanel(object,accent);
 auto* caption=lv_obj_get_child(object,0);lv_obj_set_style_text_font(caption,&lv_font_montserrat_14,0);
 squareSettingsRemember(object,id);return object;
}
static void squareDeckHeader(const char* title,const char* detail){
 lv_obj_set_style_bg_color(root,color(uiColors().bg),0);
 const bool clockHeader=!strcmp(title,"SET TIME")||!strcmp(title,"SET DATE")||!strcmp(title,"CLOCK & SYNC")||!strcmp(title,"TIME ZONE");
 auto* eyebrow=squareLabel(root,clockHeader?"PIRATE BOX / CLOCK":"PIRATE BOX / SETTINGS",10,5,248,&lv_font_montserrat_10,uiColors().accent);
 lv_obj_set_style_text_font(eyebrow,&lv_font_montserrat_10,0);
 auto* heading=squareLabel(root,title,10,23,250,&lv_font_montserrat_20,uiColors().ink);
 lv_obj_set_style_text_font(heading,&lv_font_montserrat_20,0);
 if(detail){auto* sub=squareLabel(root,detail,10,50,250,&lv_font_montserrat_10,uiColors().muted);
   lv_obj_set_style_text_font(sub,&lv_font_montserrat_10,0);lv_label_set_long_mode(sub,LV_LABEL_LONG_WRAP);lv_obj_set_height(sub,24);}
 // Use the compact live sprite here: its native 48x64 bounds stay entirely
 // inside the header beside scrolling lists, even before LVGL applies zoom.
 pirateboxMascotShow(root,270,4,54);
 auto* rule=lv_obj_create(root);lv_obj_remove_style_all(rule);lv_obj_set_pos(rule,8,75);lv_obj_set_size(rule,304,1);
 lv_obj_set_style_bg_color(rule,color(uiColors().border),0);lv_obj_set_style_bg_opa(rule,LV_OPA_COVER,0);
 lv_obj_clear_flag(rule,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
}
static void squareDeckFooter(int backId=SquareSettingsBack,int saveId=0,const char* saveText="SAVE"){
 if(saveId){
   squareDeckButton(root,"HOME",8,282,68,30,197,uiColors().border);
   squareDeckButton(root,"BACK",82,282,68,30,backId,uiColors().border);
   squareDeckButton(root,saveText,156,282,156,30,saveId,uiColors().accent);
 }else{
   squareDeckButton(root,"HOME",8,282,148,30,197,uiColors().border);
   squareDeckButton(root,"BACK",164,282,148,30,backId,uiColors().accent);
 }
}
static lv_obj_t* squareDeckBody(){
 auto* object=lv_obj_create(root);lv_obj_remove_style_all(object);lv_obj_set_pos(object,8,81);lv_obj_set_size(object,304,193);
 lv_obj_set_scroll_dir(object,LV_DIR_VER);lv_obj_set_scrollbar_mode(object,LV_SCROLLBAR_MODE_AUTO);
 lv_obj_set_style_pad_right(object,5,0);lv_obj_set_style_pad_bottom(object,3,0);
 lv_obj_set_style_bg_color(object,color(uiColors().accent),LV_PART_SCROLLBAR);
 lv_obj_set_style_width(object,3,LV_PART_SCROLLBAR);
 squareListY=0;body=object;return object;
}
static lv_obj_t* squareDeckRow(lv_obj_t* parent,const char* top,const char* detail,int id,uint32_t accent=0,bool selected=false){
 if(!accent)accent=uiColors().border;
 lv_point_t headingSize{},detailSize{};
 lv_txt_get_size(&headingSize,top,&lv_font_montserrat_16,0,0,263,LV_TEXT_FLAG_NONE);
 lv_txt_get_size(&detailSize,detail,&lv_font_montserrat_12,0,0,270,LV_TEXT_FLAG_NONE);
 const int height=std::max(57,int(headingSize.y)+int(detailSize.y)+23);
 auto* object=squareDeckButton(parent,"",2,squareListY,296,height,id,accent);
 auto* rail=lv_obj_create(object);lv_obj_remove_style_all(rail);lv_obj_set_pos(rail,0,0);lv_obj_set_size(rail,3,height-2);
 lv_obj_set_style_bg_color(rail,color(selected?uiColors().accent:accent),0);lv_obj_set_style_bg_opa(rail,LV_OPA_COVER,0);
 lv_obj_clear_flag(rail,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
 auto* heading=squareLabel(object,top,12,9,263,&lv_font_montserrat_16,uiColors().ink);
 lv_obj_set_style_text_font(heading,&lv_font_montserrat_16,0);lv_label_set_long_mode(heading,LV_LABEL_LONG_WRAP);
 lv_obj_set_height(heading,headingSize.y);
 auto* sub=squareLabel(object,detail,12,13+headingSize.y,270,&lv_font_montserrat_12,uiColors().muted);
 lv_obj_set_style_text_font(sub,&lv_font_montserrat_12,0);lv_label_set_long_mode(sub,LV_LABEL_LONG_WRAP);lv_obj_set_height(sub,detailSize.y);
 if(selected){lv_obj_set_style_border_color(object,color(uiColors().accent),0);lv_obj_set_style_border_width(object,2,0);}
 squareListY+=height+6;return object;
}
static void squareClockTabs(unsigned active){
 const char* names[]={"TIME","DATE","SYNC"};const int ids[]={160,162,163};
 for(unsigned n=0;n<3;++n){auto* tab=squareDeckButton(root,names[n],8+n*103,82,98,30,ids[n],uiColors().border);
   if(n==active){lv_obj_set_style_border_color(tab,color(uiColors().accent),0);lv_obj_set_style_border_width(tab,2,0);}}
}
static void buildSquareClockWheels(){
 auto& d=clockEditor.draft;clockEditor.clampDay();dismissNotice();
 squareDeckHeader(clockDateTab?"SET DATE":"SET TIME","BOOT edits / L-R adjusts / hold exits the field.");
 squareClockTabs(clockDateTab?1:0);
 const int fields[]={clockDateTab?1:3,clockDateTab?2:4,clockDateTab?0:5};
 const int first[]={2000,1,1,1,0,0},last[]={2099,12,dragon::monthDays(d.year,d.month),12,59,1};
 const int current[]={d.year,d.month,d.day,dragon::hour12(d.hour),d.minute,d.hour>=12};
 const char* names[]={"YEAR","MONTH","DAY","HOUR","MINUTE","AM / PM"};
 for(int col=0;col<3;++col){int field=fields[col],x=8+col*103;
   auto* caption=squareLabel(root,names[field],x,123,98,&lv_font_montserrat_12,uiColors().accent);
   lv_obj_set_style_text_font(caption,&lv_font_montserrat_12,0);lv_obj_set_style_text_align(caption,LV_TEXT_ALIGN_CENTER,0);
   auto* wheel=dateWheels[field]=lv_roller_create(root);lv_obj_set_pos(wheel,x,145);lv_obj_set_width(wheel,98);
   squareDeckPanel(wheel,uiColors().border);lv_obj_set_style_text_font(wheel,&lv_font_montserrat_24,0);
   lv_obj_set_style_text_line_space(wheel,8,0);lv_obj_set_style_text_color(wheel,color(uiColors().muted),0);
   lv_obj_set_style_bg_color(wheel,color(uiColors().raised),LV_PART_SELECTED);
   lv_obj_set_style_text_color(wheel,color(uiColors().ink),LV_PART_SELECTED);
   lv_roller_set_visible_row_count(wheel,3);
   if(field==5){lv_roller_set_options(wheel,"AM\nPM",LV_ROLLER_MODE_NORMAL);lv_roller_set_selected(wheel,current[field],LV_ANIM_OFF);}
   else if(field==1){lv_roller_set_options(wheel,"Jan\nFeb\nMar\nApr\nMay\nJun\nJul\nAug\nSep\nOct\nNov\nDec",LV_ROLLER_MODE_NORMAL);lv_roller_set_selected(wheel,d.month-1,LV_ANIM_OFF);}
   else wheelOptions(wheel,first[field],last[field],current[field]);
   lv_obj_add_event_cb(wheel,wheelChanged,LV_EVENT_VALUE_CHANGED,(void*)(intptr_t)field);squareSettingsRemember(wheel,7400+field);
 }
 auto* note=squareLabel(root,wallTime()?"Save applies the selected time or date.":"Set both time and date to start the clock.",10,258,300,&lv_font_montserrat_10,uiColors().muted);
 lv_obj_set_style_text_font(note,&lv_font_montserrat_10,0);
 squareDeckFooter(164,141,wallTime()||(clockDateTab?clockEditor.timeConfirmed:clockEditor.dateConfirmed)?"SAVE":clockDateTab?"NEXT: TIME":"NEXT: DATE");
}
static void buildSquareClockSync(){
 dismissNotice();squareDeckHeader("CLOCK & SYNC","RTC time, computer sync and your local UTC offset.");squareClockTabs(2);
 clockValue=squareLabel(root,"",10,132,300,&lv_font_montserrat_24,uiColors().ink);centered(clockValue);lv_obj_set_style_text_font(clockValue,&lv_font_montserrat_24,0);
 clockDate=squareLabel(root,"",10,168,300,&lv_font_montserrat_14,uiColors().accent);centered(clockDate);updateClockStatus();
 auto* detail=squareLabel(root,"Connect by USB to sync from your computer.\nUse Time and Date for manual setup.",12,202,296,&lv_font_montserrat_12,uiColors().muted);
 lv_obj_set_style_text_font(detail,&lv_font_montserrat_12,0);lv_label_set_long_mode(detail,LV_LABEL_LONG_WRAP);
 squareDeckButton(root,"TIME ZONE",8,240,304,32,161,uiColors().accent);squareDeckFooter(164);
}
static void buildSquareClockZone(){
 dismissNotice();squareDeckHeader("TIME ZONE","Choose your UTC offset in 15-minute steps.");
 editValue=squareLabel(root,"",10,100,300,&lv_font_montserrat_32,uiColors().ink);centered(editValue);lv_obj_set_style_text_font(editValue,&lv_font_montserrat_32,0);updateClockZone();
 auto* minus=squareDeckButton(root,"- 15 MIN",8,155,148,54,137,uiColors().border);
 auto* plus=squareDeckButton(root,"+ 15 MIN",164,155,148,54,138,uiColors().accent);
 lv_obj_add_event_cb(minus,action,LV_EVENT_LONG_PRESSED_REPEAT,(void*)137);lv_obj_add_event_cb(plus,action,LV_EVENT_LONG_PRESSED_REPEAT,(void*)138);
 auto* detail=squareLabel(root,"This changes displayed local time.\nUSB sync can use your computer's time zone.",12,228,296,&lv_font_montserrat_12,uiColors().muted);
 lv_obj_set_style_text_font(detail,&lv_font_montserrat_12,0);lv_label_set_long_mode(detail,LV_LABEL_LONG_WRAP);
 squareDeckFooter(163,171);
}
static bool buildSquareSettingsPage(){
 switch(page){
 case Settings:case RadarStyle:case Display:case Detectors:case FamilyColors:case Orientation:
 case ExternalInput:case ClockSettings:case ClockWheels:case ClockEdit:case Rules:case Controls:
 case Diagnostics:case About:break;
 default:return false;
 }
 squareSettingsActive=true;squareSettingsBuiltPage=page;squareSettingsBuiltSection=squareSettingsSection;squareSettingsRestore=true;
 if(page==ClockWheels){buildSquareClockWheels();return true;}
 if(page==ClockSettings){buildSquareClockSync();return true;}
 if(page==ClockEdit){buildSquareClockZone();return true;}
 if(page==Settings&&squareSettingsSection<0){
   squareDeckHeader("SHIP'S SETTINGS","Tune your lookout, display, clock and controls.");
   const char* names[]={"LOOK & FEEL","TIME & DATE","DETECTION","DATA & PRIVACY","RADIO & GPS","CONTROLS"};
   const char* descriptions[]={"Themes / display / angle","Clock / RTC / time zone","Families / quiet / alerts","History / files / rules","BLE / GPS / health","Encoder / stick / buttons"};
   const int ids[]={7300,7301,7302,7303,7304,191};
   const uint32_t accents[]={0xffc44d,0x6ecfff,0xff775a,0x7bda82,0xc49bff,0x42e2d4};
   for(unsigned n=0;n<6;++n){
     auto* card=squareDeckButton(root,"",8+(n%2)*156,84+(n/2)*63,148,57,ids[n],accents[n]);
     auto* name=squareLabel(card,names[n],8,9,132,&lv_font_montserrat_14,uiColors().ink);lv_obj_set_style_text_font(name,&lv_font_montserrat_14,0);
     auto* detail=squareLabel(card,descriptions[n],8,34,132,&lv_font_montserrat_8,uiColors().muted);lv_obj_set_style_text_font(detail,&lv_font_montserrat_8,0);
   }
   squareDeckFooter();return true;
 }
 const char* title=page==Settings?(squareSettingsSection==0?"LOOK & FEEL":squareSettingsSection==1?"TIME & DATE":squareSettingsSection==2?"DETECTION":squareSettingsSection==3?"DATA & PRIVACY":"RADIO & GPS"):
 page==RadarStyle?"THEMES & RADAR":page==Display?"DISPLAY":page==Detectors?"DETECTION FILTERS":page==FamilyColors?"DEVICE COLORS":page==Orientation?"ORIENTATION":page==ExternalInput?"EXPANSION CONTROLS":page==Rules?"WATCH / IGNORE":page==Controls?"TARGET CONTROLS":page==Diagnostics?"RADIO HEALTH":"ABOUT PIRATE BOX";
 squareDeckHeader(title,page==RadarStyle?"Your colors, with the full square Pirate Box layout.":page==Diagnostics?"BOOT: read / L-R scroll / BOOT exits":"L/R: move   BOOT: select   Hold: back");
 auto* list=squareDeckBody();
 auto add=[&](const char* top,const char* detail,int id,uint32_t accent=0,bool selected=false){return squareDeckRow(list,top,detail,id,accent,selected);};
 if(page==Settings){
   if(squareSettingsSection==0){
     add("Themes & radar",radarThemeNames[radarTheme],117,uiColors().accent);
     add("Display & timeouts","Brightness, dimming and screen off",71);
     add("Screen orientation","Save a fixed display angle",135);
     add("Device color legend","See the color for each detection family",150);
     add("Open radar","Return to the live Pirate lookout",1);
   }else if(squareSettingsSection==1){
     add("Set time","Hours, minutes and AM / PM",160);add("Set date","Month, day and year",162);
     add("Clock sync & time zone","RTC / computer sync / local UTC offset",163);
     add("Open clock","Choose from the square watch faces",133);
   }else if(squareSettingsSection==2){
     add("Detection filters","Families and alert confidence threshold",61,uiColors().accent);
     add(candidatesOnly?"Radar: candidates":"Radar: all contacts","Filtered candidates or all nearby traffic",53);
     String quiet=detector.autoQuiet?String("Auto quiet: after ")+detector.autoQuiet:"Auto quiet: off";
     add(quiet.c_str(),"Re-alert at +7 dB; reset after 30 minutes",72);
     add(detector.quiet?"Quiet mode: ON":"Quiet mode: OFF","Keep listening and saving observations",73);
     add("Signal smoothing",detector.smoothingMs==600?"Fast / 0.6 seconds":detector.smoothingMs==1800?"Balanced / 1.8 seconds":"Steady / 4 seconds",76);
     add("Audio hardware","No onboard speaker; visual alerts remain available",31);
   }else if(squareSettingsSection==3){
     add(detector.privacy?"Display privacy: ON":"Display privacy: OFF","Mask addresses, names and coordinates",75);
     add(detector.history?"Flash history: ON":"Flash history: OFF","512 unique records / USB CSV export",74);
     add("Watch / ignore list","Saved and snoozed device addresses",64);
     add("Recon notebook","Passive Wi-Fi / BLE details and USB export",153);
     add("Capture studio",sdReady?"Wi-Fi + BLE / SD streaming":"RAM capture / Stop then Save",7105);
     add("Saved captures","Browse recordings and download by USB",7104);
#if BOARD_HAS_SD
     add(loggingEnabled&&sdReady?"SD event logging: ON":"SD event logging: OFF",sdReady?sessionPath.c_str():"Requires a mounted SD card",43);
#endif
   }else{
     add(activeBle?"BLE scan: ACTIVE":"BLE scan: PASSIVE",activeBle?"Requests scan-response names; transmits":"Listen only; select to request extra names",98);
     add("GPS setup","External receiver / preset, pin and baud",7110);
     add("Radio health","Reception, queues and memory diagnostics",99);
     add(foxTransmit()?"Fox beacon: TRANSMITTING":"Fox beacon: OFF","Compatible public training beacon",77);
     add("Detection stays on","Menus and badge keep both radios scanning",99);
     add("About Pirate Box","Firmware version, credits and limitations",41);
   }
 }else if(page==RadarStyle){
   for(unsigned n=0;n<RadarThemeCount;++n){if(!radarThemeAllowed(n)||(n==dragon::PersonalThemeId&&!personalThemeReady()))continue;
     String name=String(radarTheme==n?"ACTIVE / ":"")+radarThemeNames[n];add(name.c_str(),radarThemeDescriptions[n],9000+n,radarPalette(n).dot,radarTheme==n);}
   add("Device color legend","Family colors stay distinct in every theme",150);
   add(radarLabels?"Device labels: ON":"Device labels: OFF","Priority tags with all detected dots visible",121);
   add(radarRecent?"Recent signals: 2 minutes":"Recent signals: OFF","Stale dots retain their last-seen age",124);
 }else if(page==Display){
   String value=String("Brightness: ")+brightness+"%";add(value.c_str(),"25 / 50 / 75 / 100 percent",44);
   value=detector.dimSeconds?String("Dim after ")+detector.dimSeconds+" seconds":"Auto dim: NEVER";add(value.c_str(),"Never / 15 / 30 / 60 / 120 / 300 seconds",80);
   value=String("Dim brightness: ")+detector.idleBrightness+"%";add(value.c_str(),"5 / 8 / 15 / 25 percent",81);
   value=detector.offSeconds?String("Screen off after ")+detector.offSeconds+" seconds":"Screen off: NEVER";add(value.c_str(),"Never / 30 / 60 / 120 / 300 / 600 seconds",82);
   add(detector.wakeAlert?"Wake on alert: ON":"Wake on alert: OFF","A button also wakes the display",83);
   add("Restore always-on defaults","Full brightness, no dimming or timeout",84);
 }else if(page==Detectors){
   add(detector.minimum==Confidence::Low?"Other alerts: all matches":detector.minimum==Confidence::Medium?"Other alerts: medium +":"Other alerts: strong only","Priority families bypass this threshold",62);
   add(axonPriority?"Axon priority: ON":"Axon priority: OFF","First AXEOFF / Axon match",151);
   for(unsigned n=2;n<unsigned(Family::Count);++n){auto family=Family(n);String name=String(detector.enabled(family)?"ON / ":"OFF / ")+dragon::familyName(family);
     add(name.c_str(),"Select to enable or disable this family",4000+n,dragon::familyColor(family),detector.enabled(family));}
 }else if(page==FamilyColors){
   for(unsigned n=0;n<unsigned(Family::Count);++n)add(dragon::familyName(Family(n)),"Each device receives its own shade",7500+n,dragon::familyColor(Family(n)));
 }else if(page==Orientation){
   for(int n=0;n<4;++n){char text[48];bool current=((4-displayRotation)&3)==n;snprintf(text,sizeof(text),"%s%d degrees",current?"ACTIVE / ":"",n*90);add(text,"Clockwise; retained after restart",144+n,0,current);}
 }else if(page==ExternalInput){
   auto config=boardC5InputConfig();const char* names[]={"Built-in buttons only","Rotary encoder","5-way joystick","Five individual buttons"};
   char encoder[90],buttons[110];snprintf(encoder,sizeof(encoder),"EXIO%u=A / EXIO%u=B / EXIO%u=press",config.pins[0],config.pins[1],config.pins[2]);
   snprintf(buttons,sizeof(buttons),"EXIO%u / %u / %u / %u / %u: up / down / left / right / OK",config.pins[0],config.pins[1],config.pins[2],config.pins[3],config.pins[4]);
   for(unsigned n=0;n<4;++n){String name=String(unsigned(config.mode)==n?"ACTIVE / ":"")+names[n];add(name.c_str(),n==1?encoder:n>1?buttons:"KeyL, KeyR and BOOT remain available",192+n,0,unsigned(config.mode)==n);}
   add("Wiring & custom mappings","Use 3.3 V pull-ups. USB CLI configures EXIO4-14; these are expander pins, not ESP32 GPIOs.",7510);
 }else if(page==Rules){
   unsigned n=0;for(size_t i=0;i<RULE_LIMIT;++i){uint8_t flags;dragon::DeviceKey key;if(!ruleAt(i,key,flags))continue;ruleKeys[n]=key;char mac[18];displayMac(key.mac,mac);
     String detail=String(flags&RuleWatch?"WATCH ":"")+(flags&RuleIgnore?"IGNORED ":"")+(flags&RuleSnooze?"SNOOZED":"");add(mac,detail.c_str(),5000+n++);}
   if(!n)add("No saved rules","Open a target's Controls to watch, ignore or snooze it.",7511);
 }else if(page==Controls){
   char mac[18];displayMac(controlKey.mac,mac);uint8_t flags=deviceRules(controlKey);
   add(mac,"Rules match this radio and address only",7512);
   add(flags&RuleWatch?"Watch: ON":"Watch: OFF","Priority when this address is observed",65);
   add(flags&RuleIgnore?"Ignore: ON":"Ignore: OFF","Mute alerts and hide from radar",66);
   add(flags&RuleSnooze?"Snoozed until restart":"Snooze until restart","Select to toggle temporary silence",67);
   add("Hunt this address","Address rotation can lose the target",68);
 }else if(page==Diagnostics){
    rotaryUiScrollRegion(list);
   auto* panel=add("Live reception","",7513);
   // Follow the live text's measured height, so scrolling ends at the last
   // diagnostic line instead of a large empty fixed-height panel.
   lv_obj_set_style_min_height(panel,57,0);lv_obj_set_height(panel,LV_SIZE_CONTENT);
   diagnosticsLabel=squareLabel(panel,"Reading...",12,39,270,&lv_font_montserrat_12,uiColors().ink);
   lv_obj_set_style_text_font(diagnosticsLabel,&lv_font_montserrat_12,0);lv_label_set_long_mode(diagnosticsLabel,LV_LABEL_LONG_WRAP);
 }else if(page==About){
   add(FIRMWARE_NAME " " FIRMWARE_VERSION,"Your pocket lookout / by Your Pal Kal",7514,uiColors().accent);
   add("Square compass radar","Angles are decorative. Signal strength does not give distance or bearing.",7515);
   add("Detection limits","Matches can be spoofed. They do not establish ownership, activity or intent.",7516);
   add("Source & credits","Dragon Detector " DRAGON_VERSION " / Valleytech. Original code: MIT. Full notices in CREDITS.md.",7517);
   add("Acknowledgements","Flock Noir / Colonel Panic; OUI Spy / Unified Blue; Marauder / Just Call Me Koko; SquachWatch; OpenDroneID (Apache-2.0).",7518);
 }
 squareDeckFooter();return true;
}
static bool squareSelectionPage(){
 switch(page){
 case Devices:case Details:case History:case HistoryDetail:case Files:case Notebook:
 case NotebookProbes:case NotebookBle:case NotebookDetail:case Alerts:case Channels:
 case Activity:case Audio:case Laps:return true;
 default:return false;
 }
}
static bool squareSelectionReadingPage(){
 // These pages have long evidence/statistics text, sometimes after action rows.
 // File lists and other selectable snapshots keep ordinary row navigation.
 return page==Details||page==HistoryDetail||page==NotebookDetail||page==Activity||page==Notebook;
}
static void squareSelectionTitle(const char* title,const char* detail){
 squareDeckHeader(title,squareSelectionReadingPage()?"BOOT: read / L-R scroll / BOOT exits":detail?detail:"Choose an entry with KeyL / KeyR and BOOT.");
 // Lists are part of the lookout, rather than the settings hierarchy.
 auto* eyebrow=lv_obj_get_child(root,0);
 if(eyebrow&&lv_obj_check_type(eyebrow,&lv_label_class))lv_label_set_text(eyebrow,"PIRATE BOX / LOOKOUT");
}
static lv_obj_t* squareSelectionBody(){
 auto* object=squareDeckBody();
 if(page==Devices||page==History||page==NotebookProbes||page==NotebookBle||page==Details||page==HistoryDetail||page==Activity||page==Laps)
   lv_obj_set_height(object,151);
 if(squareSelectionReadingPage())rotaryUiScrollRegion(object);
 return object;
}
static void squareSelectionRow(lv_obj_t* parent,const char* top,const char* detail,int id,uint32_t accent){
 squareDeckRow(parent,top,detail,id,accent==uiColors().ink?uiColors().border:accent);
}
static lv_obj_t* squareSelectionLabel(lv_obj_t* parent,const char* text,int x,int y,int width,const lv_font_t* font,uint32_t ink){
 if(parent==body){
   int top=std::max(y,squareListY?squareListY+4:0);
   auto* object=squareLabel(parent,text,8,top,280,font,ink);
   lv_obj_set_style_text_font(object,font,0);lv_label_set_long_mode(object,LV_LABEL_LONG_WRAP);
   return object;
 }
 if(parent==root&&y==280&&(page==Devices||page==NotebookProbes||page==NotebookBle)){
   auto* object=squareLabel(root,text,64,250,56,&lv_font_montserrat_12,uiColors().muted);
   lv_obj_set_style_text_font(object,&lv_font_montserrat_12,0);centered(object);return object;
 }
 return nullptr;
}
static lv_obj_t* squareSelectionButton(lv_obj_t* parent,const char* text,int x,int y,int width,int height,int id){
 if(parent==body&&(id==7001||id==7002))return squareDeckButton(body,text,2,id==7001?0:44,296,36,id,uiColors().accent);
 if(parent!=root)return nullptr;
 int left=-1,span=0;const char* caption=text;
 if(page==Devices||page==NotebookProbes||page==NotebookBle){
   if(id==114||id==156){left=8;span=50;caption="PREV";}
   if(id==115||id==157){left=126;span=50;caption="NEXT";}
   if(id==25||id==158){left=182;span=130;caption="REFRESH";}
 }else if(page==History){
   if(id==110){left=8;span=50;caption="PREV";}
   if(id==111){left=64;span=50;caption="NEXT";}
   if(id==22){left=120;span=92;caption="EVENTS";}
   if(id==112){left=218;span=94;caption="CLEAR";}
 }else if(page==Details||page==HistoryDetail){
   if(id==56||id==69){left=8;span=148;}
   if(id==52||id==70){left=164;span=148;}
 }else if(page==Activity){
   if(id==22){left=8;span=148;}if(id==23){left=164;span=148;}
 }else if(page==Channels&&id==200){
   // Auto is a normal full-width selectable row, not a floating round-footer button.
   return squareDeckRow(body,"Automatic channel selection","Resume normal channel sweeps",200,uiColors().accent);
 }else if(page==Laps&&id==130){left=8;span=304;}
 if(left<0)return nullptr;
 auto* object=squareDeckButton(root,caption,left,241,span,32,id,uiColors().accent);
 if(span<60)lv_obj_set_style_text_font(lv_obj_get_child(object,0),&lv_font_montserrat_12,0);
 return object;
}
static void buildSquareWeather(){
 squareHeading("WEATHER STATION","A live look at the air around you");
 auto card=[&](int x,const char* title,uint32_t accent){auto* box=lv_obj_create(root);lv_obj_set_pos(box,x,65);lv_obj_set_size(box,146,96);lv_obj_set_style_pad_all(box,0,0);lv_obj_set_style_bg_color(box,color(uiColors().surface),0);lv_obj_set_style_border_color(box,color(accent),0);lv_obj_set_style_radius(box,10,0);lv_obj_clear_flag(box,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);squareLabel(box,title,10,9,126,&lv_font_montserrat_12,accent);return box;};
 auto* temperature=card(10,"TEMPERATURE",0xffc44d);auto* humidity=card(164,"HUMIDITY",0x42e2d4);
 weatherTemperature=squareLabel(temperature,"--.- C",10,37,126,&lv_font_montserrat_24,uiColors().ink);
 weatherFahrenheit=squareLabel(temperature,"--.- F",10,71,126,&lv_font_montserrat_12,uiColors().muted);
 weatherHumidity=squareLabel(humidity,"--.- %",10,37,126,&lv_font_montserrat_24,uiColors().ink);
 weatherClock=squareLabel(root,"Clock not set",12,174,296,&lv_font_montserrat_16,uiColors().ink);
 weatherHardware=squareLabel(root,"Reading board sensors...",12,201,296,&lv_font_montserrat_12,uiColors().muted);
 lv_label_set_long_mode(weatherHardware,LV_LABEL_LONG_WRAP);lv_obj_set_height(weatherHardware,66);
 squareButton(root,"CLOCK",10,279,146,32,133,0x42e2d4);squareButton(root,"HOME",164,279,146,32,197,0xffc44d);
}
static void updateSquareWeather(){
 if(!weatherTemperature)return;auto sensors=boardC5Sensors();char value[32],clock[70],hardware[200];
 if(sensors.weatherValid)snprintf(value,sizeof(value),"%.1f C",sensors.temperatureC);else strcpy(value,"--.- C");lv_label_set_text(weatherTemperature,value);
 if(sensors.weatherValid)snprintf(value,sizeof(value),"%.1f F",sensors.temperatureC*1.8f+32.f);else strcpy(value,"--.- F");lv_label_set_text(weatherFahrenheit,value);
 if(sensors.weatherValid)snprintf(value,sizeof(value),"%.1f %%",sensors.humidityPercent);else strcpy(value,"--.- %%");lv_label_set_text(weatherHumidity,value);
 dragon::Calendar date;clockLocal(date);if(wallTime())snprintf(clock,sizeof(clock),"%02d:%02d:%02d   %04d-%02d-%02d",date.hour,date.minute,date.second,date.year,date.month,date.day);else strcpy(clock,"Set the clock in Settings");lv_label_set_text(weatherClock,clock);
 snprintf(hardware,sizeof(hardware),"SD: %s    RTC: %s\nBattery: %d%% / %.2f V    Motion: %s\n%s",sdReady?"ready":"missing",rtcAvailable?"ready":"missing",batteryPercent(),sensors.batteryMillivolts/1000.f,sensors.imuValid?"ready":"missing",sensors.weatherValid?"Sensor updates every 2 seconds":"Sensor unavailable; retrying");lv_label_set_text(weatherHardware,hardware);
}
void uiDetectionObserve(Contact& contact,Family family,Confidence confidence,uint32_t at){
 if(!detectionFeed||wardriveBusy()||wardriveUploadBusy())return;
 auto key=contactKey(contact);uint8_t rules=deviceRules(key);bool watch=rules&RuleWatch;
 if(!piratebox::feedObservationAllowed(contact,at,detector,!candidatesOnly||reconAll(),rules&RuleIgnore,rules&RuleSnooze,watch,reconAccepts(family,contact.ble)))return;
 if(!watch&&!detector.enabled(family))return;
 detectionFeed->observe(key,family,confidence,contact.rssi,contact.channel,contact.name,
   family==contact.match.family?contactColor(contact):dragon::familyColor(family),at);
}
static bool feedAllowed(const piratebox::FeedEntry& entry){
 uint8_t rules=deviceRules(entry.key);return piratebox::feedEntryAllowed(entry,detector,!candidatesOnly||reconAll(),rules&RuleIgnore,rules&RuleSnooze,rules&RuleWatch,reconAccepts(entry.family,entry.key.radio!=0));
}
static PirateboxMascotReaction mascotReaction(Family family){
 if(dragon::trackerFamily(family))return PirateboxMascotReaction::Tracker;
 switch(family){
 case Family::Wifi:return PirateboxMascotReaction::Wifi;
 case Family::Ble:case Family::IBeacon:return PirateboxMascotReaction::Bluetooth;
 case Family::Flock:case Family::Axon:case Family::Ring:case Family::Camera:case Family::Alpr:case Family::Raven:return PirateboxMascotReaction::Camera;
 case Family::Drone:return PirateboxMascotReaction::Drone;
 case Family::Deauth:case Family::EvilTwin:case Family::BleSpam:case Family::Skimmer:return PirateboxMascotReaction::Alert;
 default:return PirateboxMascotReaction::Curious;
 }
}
void uiDetectionMascot(lv_obj_t* parent,int x,int y,unsigned height,int captionX,int captionY,int captionWidth){
 mascotHasEncounter=false;mascotFamily=Family::Count;
 mascotCaption=squareLabel(parent,"On the lookout",captionX,captionY,captionWidth,&lv_font_montserrat_10,0xb5c8d8);
 lv_obj_set_style_text_font(mascotCaption,&lv_font_montserrat_10,0);lv_obj_set_height(mascotCaption,13);
 lv_obj_set_style_bg_color(mascotCaption,color(0),0);lv_obj_set_style_bg_opa(mascotCaption,LV_OPA_COVER,0);
 detectionMascot=pirateboxMascotShow(parent,x,y,height);
 if(detectionMascot)lv_obj_add_event_cb(detectionMascot,[](lv_event_t* event){if(lv_event_get_target(event)==detectionMascot){detectionMascot=nullptr;mascotCaption=nullptr;mascotHasEncounter=false;}},LV_EVENT_DELETE,nullptr);
 updateDetectionMascot();
}
static void updateDetectionMascot(){
 if(!detectionMascot||!mascotCaption)return;
 const piratebox::FeedEntry* latest=nullptr;
 const uint32_t now=millis();
 for(size_t n=detectionFeed?detectionFeed->count:0;n>0;--n){auto& entry=detectionFeed->entries[n-1];if(uint32_t(now-entry.lastAt)<30000&&feedAllowed(entry)){latest=&entry;break;}}
 if(!latest){if(mascotHasEncounter){pirateboxMascotReact(detectionMascot,PirateboxMascotReaction::Idle);lv_label_set_text(mascotCaption,"On the lookout");lv_obj_set_style_text_color(mascotCaption,color(0xb5c8d8),0);}mascotHasEncounter=false;return;}
 // Encounter identity remains stable while repeated advertisements coalesce.
 if(mascotHasEncounter&&mascotKey==latest->key&&mascotFamily==latest->family&&mascotEncounterAt==latest->firstAt)return;
 // Keep one reaction readable in busy rooms. Rules take effect immediately;
 // new encounters remain in the bounded feed while the current pose holds.
 if(mascotHasEncounter&&uint32_t(now-mascotReactionStartedAt)<2000){
   for(size_t n=0;n<detectionFeed->count;++n){auto& entry=detectionFeed->entries[n];if(entry.key==mascotKey&&entry.family==mascotFamily&&entry.firstAt==mascotEncounterAt&&uint32_t(now-entry.lastAt)<30000&&feedAllowed(entry))return;}
 }
 mascotHasEncounter=true;mascotKey=latest->key;mascotFamily=latest->family;mascotEncounterAt=latest->firstAt;
 mascotReactionStartedAt=now;
 char caption[64];snprintf(caption,sizeof(caption),"Saw %s",radarFamilyName(latest->family));lv_label_set_text(mascotCaption,caption);
 lv_obj_set_style_text_color(mascotCaption,color(latest->color),0);pirateboxMascotReact(detectionMascot,mascotReaction(latest->family));
}
static void updateDetectionFeed(bool force=false){
 if(!feedTitle||(!force&&uint32_t(millis()-feedRefreshAt)<250))return;feedRefreshAt=millis();
 int visible[3]{-1,-1,-1};unsigned found=0;
 for(size_t n=detectionFeed?detectionFeed->count:0;n>0&&found<3;--n)if(feedAllowed(detectionFeed->entries[n-1]))visible[2-found++]=int(n-1);
 char heading[60];snprintf(heading,sizeof(heading),"ON WATCH  /  %u recent encounters",unsigned(detectionFeed?detectionFeed->count:0));lv_label_set_text(feedTitle,heading);
 for(unsigned row=0;row<3;++row){
   if(visible[row]<0){lv_obj_add_flag(feedRows[row],LV_OBJ_FLAG_HIDDEN);continue;}
   lv_obj_clear_flag(feedRows[row],LV_OBJ_FLAG_HIDDEN);auto& entry=detectionFeed->entries[visible[row]];
   char identity[42],title[95],meta[95];piratebox::feedIdentity(entry,detector.privacy,identity,sizeof(identity));
   snprintf(title,sizeof(title),"%s / %s",radarFamilyName(entry.family),identity);
   snprintf(meta,sizeof(meta),"%s   %d dBm   %lus ago   x%u",entry.key.radio?"BLE":"Wi-Fi",entry.rssi,static_cast<unsigned long>((millis()-entry.lastAt)/1000),entry.observations);
   lv_label_set_text(feedNames[row],title);lv_label_set_text(feedMeta[row],meta);
   lv_obj_set_style_text_color(feedNames[row],color(entry.color),0);lv_obj_set_style_bg_color(feedBars[row],color(entry.color),0);
 }
 if(!found){lv_obj_clear_flag(feedRows[2],LV_OBJ_FLAG_HIDDEN);lv_label_set_text(feedNames[2],detectionFeed?"Listening for detections...":"Feed memory unavailable");lv_label_set_text(feedMeta[2],"Signals appear here; no popups to dismiss");lv_obj_set_style_text_color(feedNames[2],color(0xc0d4df),0);lv_obj_set_style_bg_color(feedBars[2],color(0x42e2d4),0);}
}
static void buildDetectionFeed(){
 auto* panel=lv_obj_create(root);lv_obj_remove_style_all(panel);lv_obj_set_pos(panel,0,piratebox::FeedTop);lv_obj_set_size(panel,320,piratebox::FeedHeight);
 lv_obj_set_style_bg_color(panel,color(0x020a14),0);lv_obj_set_style_bg_opa(panel,235,0);lv_obj_set_style_border_side(panel,LV_BORDER_SIDE_TOP,0);lv_obj_set_style_border_width(panel,1,0);lv_obj_set_style_border_color(panel,color(0x258a93),0);lv_obj_clear_flag(panel,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
 feedTitle=squareLabel(panel,"ON WATCH",9,1,302,&lv_font_montserrat_8,0x8fb5c6);
 lv_obj_set_style_text_font(feedTitle,&lv_font_montserrat_8,0);
 for(unsigned row=0;row<3;++row){
   auto* entry=feedRows[row]=lv_obj_create(panel);lv_obj_remove_style_all(entry);lv_obj_set_pos(entry,8,11+row*22);lv_obj_set_size(entry,304,22);lv_obj_clear_flag(entry,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
   feedBars[row]=lv_obj_create(entry);lv_obj_remove_style_all(feedBars[row]);lv_obj_set_pos(feedBars[row],0,2);lv_obj_set_size(feedBars[row],3,18);lv_obj_set_style_bg_opa(feedBars[row],LV_OPA_COVER,0);lv_obj_clear_flag(feedBars[row],LV_OBJ_FLAG_CLICKABLE);
   feedNames[row]=squareLabel(entry,"",8,0,294,&lv_font_montserrat_10,0x42e2d4);
   feedMeta[row]=squareLabel(entry,"",8,12,294,&lv_font_montserrat_8,0xa9bdca);
   lv_obj_set_style_text_font(feedNames[row],&lv_font_montserrat_10,0);
   lv_obj_set_style_text_font(feedMeta[row],&lv_font_montserrat_8,0);
 }
 updateDetectionFeed(true);
}
#endif


