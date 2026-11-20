#if defined(BOARD_CROWPANEL_21) || defined(BOARD_WATCHER) || defined(BOARD_WS_C5_273)
#include "app.h"
#include "rotary_ui.h"
#include "rotary_input.h"
#include "ui_theme.h"

namespace {
lv_group_t* group=nullptr;
lv_obj_t* members[160]{};
unsigned memberCount=0;
#ifdef BOARD_WS_C5_273
// Only an explicitly registered text region joins the navigation group.
// Ordinary lists retain their existing per-row focus and Select actions.
lv_obj_t* readingRegion=nullptr;
void readingEvent(lv_event_t* event){
  auto* object=lv_event_get_target(event);if(object!=readingRegion)return;
  if(lv_event_get_code(event)==LV_EVENT_DELETE)readingRegion=nullptr;
  else if(lv_event_get_code(event)==LV_EVENT_CLICKED){
    // The first encoder Enter enters native LVGL edit mode without CLICKED.
    // The next Enter clicks the region, returning to normal focus navigation.
    auto* owner=static_cast<lv_group_t*>(lv_obj_get_group(object));
    if(owner)lv_group_set_editing(owner,false);
  }
}
#endif
#ifdef BOARD_WATCHER
dragon::RotaryNavigation navigation(350,140);
#else
dragon::RotaryNavigation navigation;
#endif
unsigned pendingClicks=0;
bool releasing=false;
#if defined(BOARD_WATCHER) || defined(BOARD_WS_C5_273)
lv_indev_t* encoderInput=nullptr;
#endif
#ifdef BOARD_WATCHER
bool touchActive=false,touchSeen=false;
uint32_t touchAt=0;
uint32_t activeKey=LV_KEY_ENTER;
bool touchOwnsInput(){return touchActive||(touchSeen&&uint32_t(millis()-touchAt)<150);}
#endif
#if defined(BOARD_WATCHER) || defined(BOARD_WS_C5_273)
void cancelEncoder(){
  navigation.clear();pendingClicks=0;releasing=false;
  if(encoderInput)lv_indev_reset(encoderInput,nullptr);
}
#endif

lv_obj_t* dialog(lv_obj_t* obj){
  if(!obj||lv_obj_has_flag(obj,LV_OBJ_FLAG_HIDDEN))return nullptr;
  for(int i=int(lv_obj_get_child_cnt(obj))-1;i>=0;--i)
    if(auto* box=dialog(lv_obj_get_child(obj,i)))return box;
  return lv_obj_check_type(obj,&lv_msgbox_class)?obj:nullptr;
}
bool selectable(lv_obj_t* obj){
#ifdef BOARD_WS_C5_273
  if(obj==readingRegion)return lv_obj_has_flag(obj,LV_OBJ_FLAG_SCROLLABLE);
#endif
  return lv_obj_has_flag(obj,LV_OBJ_FLAG_CLICKABLE)&&
    (lv_obj_check_type(obj,&lv_btn_class)||lv_obj_check_type(obj,&lv_btnmatrix_class)||
     lv_obj_check_type(obj,&lv_roller_class)||lv_obj_check_type(obj,&lv_dropdown_class)||
     lv_obj_check_type(obj,&lv_slider_class)||lv_obj_check_type(obj,&lv_switch_class)||
     lv_obj_check_type(obj,&lv_checkbox_class)||lv_obj_check_type(obj,&lv_textarea_class));
}
void collect(lv_obj_t* obj,lv_obj_t** found,unsigned& count){
  if(!obj||lv_obj_has_flag(obj,LV_OBJ_FLAG_HIDDEN)||lv_obj_has_state(obj,LV_STATE_DISABLED))return;
  if(selectable(obj)&&count<160)found[count++]=obj;
  for(unsigned i=0;i<lv_obj_get_child_cnt(obj);++i)collect(lv_obj_get_child(obj,i),found,count);
}
void readEncoder(lv_indev_drv_t*,lv_indev_data_t* data){
  data->key=LV_KEY_ENTER;data->enc_diff=0;
#ifdef BOARD_WATCHER
  if(touchOwnsInput()){cancelEncoder();data->state=LV_INDEV_STATE_REL;return;}
#endif
#ifdef BOARD_WATCHER
  // Finish each key press/release in one LVGL read cycle, before rendering.
  if(releasing){
    releasing=false;data->key=activeKey;data->state=LV_INDEV_STATE_REL;
  }else{
    if(pendingClicks){pendingClicks=0;activeKey=LV_KEY_ENTER;}
    else {data->key=activeKey;data->state=LV_INDEV_STATE_REL;return;}
    data->key=activeKey;data->state=LV_INDEV_STATE_PR;releasing=true;
  }
  data->continue_reading=releasing; // a selection cannot remain held across slow redraws
#else
  if(releasing){releasing=false;data->state=LV_INDEV_STATE_REL;}
  else if(pendingClicks){--pendingClicks;releasing=true;data->state=LV_INDEV_STATE_PR;}
  else {data->enc_diff=navigation.take(millis());data->state=LV_INDEV_STATE_REL;}
#ifdef BOARD_WS_C5_273
  // Complete a physical Select click before a slow frame can defer release.
  data->continue_reading=releasing;
#endif
#endif
}
}
#ifdef BOARD_WS_C5_273
void rotaryUiScrollRegion(lv_obj_t* object){
  if(!object||object==readingRegion)return;
  readingRegion=object;
  lv_obj_add_flag(object,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_WITH_ARROW);
  lv_obj_add_event_cb(object,readingEvent,LV_EVENT_ALL,nullptr);
  lv_obj_set_style_outline_color(object,lv_color_hex(uiColors().accent),LV_STATE_EDITED);
  lv_obj_set_style_outline_width(object,4,LV_STATE_EDITED);
}
#endif
void rotaryUiBegin(){
  group=lv_group_create();lv_group_set_wrap(group,true);
  static lv_indev_drv_t driver;lv_indev_drv_init(&driver);
#ifdef BOARD_WATCHER
  driver.type=LV_INDEV_TYPE_KEYPAD;
#else
  driver.type=LV_INDEV_TYPE_ENCODER;
#endif
  driver.read_cb=readEncoder;
  auto* input=lv_indev_drv_register(&driver);lv_indev_set_group(input,group);
#if defined(BOARD_WATCHER) || defined(BOARD_WS_C5_273)
  encoderInput=input;
#endif
}
void rotaryUiInput(int steps,bool click,bool back){
  if(!group||(!steps&&!click&&!back))return;
#ifdef BOARD_WATCHER
  if(touchOwnsInput()){cancelEncoder();return;}
#endif
  if(uiRotaryWake()){
#ifdef BOARD_WATCHER
    cancelEncoder();
#else
    navigation.clear();
#ifdef BOARD_WS_C5_273
    pendingClicks=0;releasing=false;
#endif
#endif
    return;
  }
  if(back){
#ifdef BOARD_WS_C5_273
    if(lv_group_get_editing(group)){
      navigation.clear();pendingClicks=0;releasing=false;
      lv_group_set_editing(group,false);return;
    }
#endif
#ifdef BOARD_WATCHER
    cancelEncoder();
#endif
    navigation.clear();pendingClicks=0;releasing=false;lv_group_set_editing(group,false);
#ifdef BOARD_WATCHER
    if(uiRotaryOpenMenu())return;
#endif
    uiKey(true);return;
  }
#ifdef BOARD_WATCHER
  if(click){pendingClicks=1;}
  else if(!pendingClicks&&!releasing){
    lv_group_set_editing(group,false);
    // Apply this frame's actual detents together, with no delayed navigation backlog.
    int count=constrain(steps,-8,8);
    while(count>0){lv_group_focus_next(group);--count;}
    while(count<0){lv_group_focus_prev(group);++count;}
  }
#elif defined(BOARD_WS_C5_273)
  if(click){navigation.clear();pendingClicks=1;}
  else if(!pendingClicks&&!releasing){
    // KeyL/KeyR and external detents are already debounced by the board.
    // Do not expire an accepted press while LVGL is drawing a large frame.
    int count=constrain(steps,-8,8);
    while(count>0){if(lv_group_get_editing(group))lv_group_send_data(group,LV_KEY_RIGHT);else lv_group_focus_next(group);--count;}
    while(count<0){if(lv_group_get_editing(group))lv_group_send_data(group,LV_KEY_LEFT);else lv_group_focus_prev(group);++count;}
  }
#else
  if(click){navigation.clear();pendingClicks=1;}
  else if(!pendingClicks&&!releasing)navigation.push(steps,millis());
#endif
}
#ifdef BOARD_WATCHER
void rotaryUiTouch(bool down){
  if(down||touchActive){touchAt=millis();touchSeen=true;}
  touchActive=down;
  if(down)cancelEncoder();
}
#endif
#if defined(BOARD_WATCHER) || defined(BOARD_WS_C5_273)
void rotaryUiReset(){
  cancelEncoder();memberCount=0;
#ifdef BOARD_WS_C5_273
  readingRegion=nullptr;
#endif
  if(group){lv_group_remove_all_objs(group);lv_group_set_editing(group,false);}
}
#endif
void rotaryUiLoop(){
  if(!group)return;
  auto* scope=uiRotaryScope();
  if(auto* box=dialog(lv_layer_top()))scope=box;
  else if(auto* box=dialog(scope))scope=box;
  lv_obj_t* found[160]{};unsigned count=0;collect(scope,found,count);
  bool changed=count!=memberCount||lv_group_get_obj_count(group)!=count;
  for(unsigned i=0;i<count&&!changed;++i)changed=found[i]!=members[i];
  if(!changed)return;
  navigation.clear();
#if defined(BOARD_WATCHER) || defined(BOARD_WS_C5_273)
  // A queued click belongs to the old widget set. Never send it to a newly
  // created button at a recycled address during a status-driven redraw.
  cancelEncoder();
#endif
  auto* focused=lv_group_get_focused(group);bool keep=false;
  for(unsigned i=0;i<count;++i)if(found[i]==focused)keep=true;
  bool editing=keep&&lv_group_get_editing(group);
  lv_group_remove_all_objs(group);memberCount=count;
  for(unsigned i=0;i<count;++i){
    auto* obj=members[i]=found[i];
    lv_obj_add_flag(obj,LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_set_style_outline_color(obj,lv_color_hex(uiColors().accent),LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(obj,3,LV_STATE_FOCUSED);
    lv_obj_set_style_outline_pad(obj,2,LV_STATE_FOCUSED);
    lv_group_add_obj(group,obj);
  }
  if(keep)lv_group_focus_obj(focused);
  lv_group_set_editing(group,editing);
}
#endif
