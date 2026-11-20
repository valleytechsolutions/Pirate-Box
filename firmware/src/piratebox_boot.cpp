#include "piratebox_boot.h"

#ifdef BOARD_WS_C5_273
#include "piratebox_boot_data.inc"

namespace {
constexpr uint32_t kIdlePeriodMs=8400;

void setSprite(lv_obj_t* image,const lv_img_dsc_t* source) {
  if(lv_img_get_src(image)==source)return;
  lv_point_t pivot;lv_img_get_pivot(image,&pivot);
  lv_img_set_src(image,source);
  lv_img_set_pivot(image,pivot.x,pivot.y);
}

struct MascotState {
  PirateboxMascotReaction reaction;
  uint32_t changedAt;
  uint32_t idleAt;
};
void deleteMascot(lv_event_t* event);

unsigned idleFrame(uint32_t elapsed) {
  // A short blink, a gentle breathing frame and an occasional friendly wave.
  const uint32_t phase=elapsed%kIdlePeriodMs;
  if(phase>=6500 && phase<7040)return 3;
  if(phase>=3600 && phase<3740)return 1;
  return (phase/1200)%2 ? 2 : 0;
}

void animateMascot(void* object,int32_t) {
  auto* image=static_cast<lv_obj_t*>(object);
  auto* state=static_cast<MascotState*>(lv_obj_get_event_user_data(image,deleteMascot));
  if(!state)return;
  const uint32_t now=lv_tick_get();
  const uint32_t elapsed=now-state->changedAt; // Wrap-safe millisecond duration.
  if(state->reaction!=PirateboxMascotReaction::Idle && elapsed>=2520) {
    state->reaction=PirateboxMascotReaction::Idle;state->idleAt=now;
  }
  unsigned frame=idleFrame(now-state->idleAt);
  if(state->reaction!=PirateboxMascotReaction::Idle) {
    unsigned first=4;
    switch(state->reaction) {
      case PirateboxMascotReaction::Wifi:first=6;break;
      case PirateboxMascotReaction::Bluetooth:first=8;break;
      case PirateboxMascotReaction::Camera:first=12;break;
      case PirateboxMascotReaction::Drone:first=10;break;
      case PirateboxMascotReaction::Alert:first=14;break;
      case PirateboxMascotReaction::Tracker:
      case PirateboxMascotReaction::Curious:first=4;break;
      case PirateboxMascotReaction::Idle:break;
    }
    frame=first+((elapsed/360)&1u);
  }
  setSprite(image,&piratebox_mascot_frames[frame]);
}

void deleteMascot(lv_event_t* event) {
  auto* image=lv_event_get_target(event);
  // DELETE runs before LVGL's object destructor. Cancel callbacks before freeing
  // the state, even when the page is closed halfway through a reaction.
  lv_anim_del(image,animateMascot);
  void* state=lv_event_get_user_data(event);
  lv_mem_free(state);
}

void animateAvatar(void* object, int32_t elapsed) {
  auto* image = static_cast<lv_obj_t*>(object);
  const lv_img_dsc_t* source = pirateboxAvatarIdleFrame(static_cast<uint32_t>(elapsed));
  if (lv_img_get_src(image) != source) {
    lv_point_t pivot;lv_img_get_pivot(image,&pivot);
    lv_img_set_src(image, source);
    // LVGL resets the pivot when an image source changes. Keep scaled sprites
    // anchored to their original corner throughout the animation.
    lv_img_set_pivot(image,pivot.x,pivot.y);
  }
}
}

const lv_img_dsc_t* pirateboxAvatarFrame(unsigned frame){return &piratebox_avatar_frames[frame%4];}
const lv_img_dsc_t* pirateboxAvatarIdleFrame(uint32_t elapsedMs){return &piratebox_avatar_frames[idleFrame(elapsedMs)];}
const lv_img_dsc_t* pirateboxTitleImage(){return &piratebox_title;}
lv_obj_t* pirateboxMascotShow(lv_obj_t* parent,int x,int y,unsigned maxHeight) {
  if(!parent || !maxHeight)return nullptr;
  auto* state=static_cast<MascotState*>(lv_mem_alloc(sizeof(MascotState)));
  if(!state)return nullptr;
  auto* mascot=lv_img_create(parent);
  if(!mascot){lv_mem_free(state);return nullptr;}
  const uint32_t now=lv_tick_get();
  *state={PirateboxMascotReaction::Idle,now,now};
  lv_img_set_src(mascot,&piratebox_mascot_frames[0]);
  lv_img_set_antialias(mascot,false);
  lv_img_set_pivot(mascot,0,0);
  const unsigned height=maxHeight>320?320:maxHeight;
  lv_img_set_zoom(mascot,(height*256)/64);
  lv_obj_set_pos(mascot,x,y);
  lv_obj_clear_flag(mascot,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(mascot,deleteMascot,LV_EVENT_DELETE,state);
  lv_anim_t animation;lv_anim_init(&animation);
  lv_anim_set_var(&animation,mascot);lv_anim_set_exec_cb(&animation,animateMascot);
  lv_anim_set_values(&animation,0,1000);lv_anim_set_time(&animation,1000);
  lv_anim_set_path_cb(&animation,lv_anim_path_linear);
  lv_anim_set_repeat_count(&animation,LV_ANIM_REPEAT_INFINITE);
  lv_anim_start(&animation);
  return mascot;
}

void pirateboxMascotReact(lv_obj_t* mascot,PirateboxMascotReaction reaction) {
  if(!mascot || !lv_obj_is_valid(mascot))return;
  // The callback identity verifies ownership without any global widget pointer.
  auto* state=static_cast<MascotState*>(lv_obj_get_event_user_data(mascot,deleteMascot));
  if(!state)return;
  state->reaction=reaction;
  state->changedAt=lv_tick_get();
  if(reaction==PirateboxMascotReaction::Idle)state->idleAt=state->changedAt;
  animateMascot(mascot,0);
}
lv_obj_t* pirateboxAvatarShow(lv_obj_t* parent,int x,int y,unsigned maxHeight){
  if(!parent||!maxHeight)return nullptr;
  auto* avatar=lv_img_create(parent);lv_img_set_src(avatar,pirateboxAvatarFrame(0));
  lv_img_set_antialias(avatar,false);
  lv_img_set_pivot(avatar,0,0);lv_img_set_zoom(avatar,(maxHeight*256)/piratebox_avatar_frames[0].header.h);
  lv_obj_set_pos(avatar,x,y);lv_obj_clear_flag(avatar,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
  lv_anim_t animation;lv_anim_init(&animation);lv_anim_set_var(&animation,avatar);
  lv_anim_set_exec_cb(&animation,animateAvatar);lv_anim_set_values(&animation,0,kIdlePeriodMs-1);
  lv_anim_set_time(&animation,kIdlePeriodMs);lv_anim_set_path_cb(&animation,lv_anim_path_linear);
  lv_anim_set_repeat_count(&animation,LV_ANIM_REPEAT_INFINITE);lv_anim_start(&animation);
  return avatar;
}

void pirateboxBootShow(lv_obj_t* parent) {
  if (!parent) return;
  lv_obj_set_style_bg_color(parent, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

  auto* avatar = lv_img_create(parent);
  lv_img_set_src(avatar, &piratebox_avatar_frames[0]);
  lv_obj_align(avatar, LV_ALIGN_TOP_MID, 0, 35);
  lv_obj_clear_flag(avatar, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  auto* title = lv_img_create(parent);
  lv_img_set_src(title, &piratebox_title);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 214);
  lv_obj_clear_flag(title, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  lv_anim_t animation;
  lv_anim_init(&animation);
  // lv_obj's destructor removes animations whose variable is that object.
  // No timers, global widget pointers, or allocated image buffers outlive it.
  lv_anim_set_var(&animation, avatar);
  lv_anim_set_exec_cb(&animation, animateAvatar);
  lv_anim_set_values(&animation, 0, kIdlePeriodMs - 1);
  lv_anim_set_time(&animation, kIdlePeriodMs);
  lv_anim_set_path_cb(&animation, lv_anim_path_linear);
  lv_anim_set_repeat_count(&animation, LV_ANIM_REPEAT_INFINITE);
  lv_anim_start(&animation);
}

void pirateboxBootTick() {
  // The application's existing lv_timer_handler() advances the animation.
  // Calling it here again could re-enter LVGL while a page is being deleted.
}
#else
void pirateboxBootShow(lv_obj_t*) {}
void pirateboxBootTick() {}
#endif
