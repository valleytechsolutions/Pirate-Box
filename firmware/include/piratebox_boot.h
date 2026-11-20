#pragma once
#include <lvgl.h>

// Adds embedded avatar sprites and the PIRATE BOX title to an empty parent.
// The parent's normal LVGL lifetime owns all objects and animation callbacks.
void pirateboxBootShow(lv_obj_t* parent);

// Kept for the boot-page tick hook. LVGL's timer advances the animation itself.
void pirateboxBootTick();

#ifdef BOARD_WS_C5_273
const lv_img_dsc_t* pirateboxAvatarFrame(unsigned frame);
const lv_img_dsc_t* pirateboxAvatarIdleFrame(uint32_t elapsedMs);
const lv_img_dsc_t* pirateboxTitleImage();
// Native pixel coordinates; the sprite is scaled to maxHeight and animated.
lv_obj_t* pirateboxAvatarShow(lv_obj_t* parent,int x,int y,unsigned maxHeight);
enum class PirateboxMascotReaction : uint8_t {
    Idle, Curious, Wifi, Bluetooth, Camera, Drone, Tracker, Alert
};
// The image owns its animation state. Clear the caller's pointer on page cleanup.
lv_obj_t* pirateboxMascotShow(lv_obj_t* parent,int x,int y,unsigned maxHeight);
void pirateboxMascotReact(lv_obj_t* mascot,PirateboxMascotReaction reaction);
#endif
