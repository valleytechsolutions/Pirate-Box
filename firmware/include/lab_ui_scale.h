#pragma once
#include "board_pins.h"
#include <lvgl.h>
namespace dragon::labui {
inline int px(int n){return n*SCREEN_RES/466;}
#ifdef BOARD_WS_C5_273
// Full-screen legacy pages keep their vertical coordinates; content is given
// the full square width instead of the former circular display's safe area.
inline int contentPx(int n){return n*SCREEN_RES/360;}
inline int objectPx(const lv_obj_t* object,int n){return lv_obj_get_parent(object)==lv_scr_act()?px(n):contentPx(n);}
inline int logical(int n){return (n*360+SCREEN_RES-1)/SCREEN_RES;}
inline void pos(lv_obj_t* o,int x,int y){lv_obj_set_pos(o,objectPx(o,x),objectPx(o,y));}
inline void size(lv_obj_t* o,int w,int h){lv_obj_set_size(o,objectPx(o,w),objectPx(o,h));}
inline void width(lv_obj_t* o,int w){lv_obj_set_width(o,objectPx(o,w));}
inline void height(lv_obj_t* o,int h){lv_obj_set_height(o,objectPx(o,h));}
#else
inline int logical(int n){return (n*466+SCREEN_RES-1)/SCREEN_RES;}
inline void pos(lv_obj_t* o,int x,int y){lv_obj_set_pos(o,px(x),px(y));}
inline void size(lv_obj_t* o,int w,int h){lv_obj_set_size(o,px(w),px(h));}
inline void width(lv_obj_t* o,int w){lv_obj_set_width(o,px(w));}
inline void height(lv_obj_t* o,int h){lv_obj_set_height(o,px(h));}
#endif
#if defined(BOARD_WS_C5_273)
inline const lv_font_t* font14(){return &lv_font_montserrat_12;}
inline const lv_font_t* font16(){return &lv_font_montserrat_14;}
inline const lv_font_t* font18(){return &lv_font_montserrat_16;}
inline const lv_font_t* font26(){return &lv_font_montserrat_20;}
#elif defined(BOARD_XIAO_ROUND)
inline const lv_font_t* font14(){return &lv_font_montserrat_8;}
inline const lv_font_t* font16(){return &lv_font_montserrat_10;}
inline const lv_font_t* font18(){return &lv_font_montserrat_10;}
inline const lv_font_t* font26(){return &lv_font_montserrat_14;}
#elif defined(BOARD_WATCHER) || defined(BOARD_LILYGO_175) || defined(BOARD_AMOLED_175C) || defined(BOARD_AMOLED_175) || defined(BOARD_STOPWATCH) || defined(BOARD_CROWPANEL_21)
inline const lv_font_t* font14(){return &lv_font_montserrat_14;}
inline const lv_font_t* font16(){return &lv_font_montserrat_16;}
inline const lv_font_t* font18(){return &lv_font_montserrat_18;}
inline const lv_font_t* font26(){return &lv_font_montserrat_26;}
#else
inline const lv_font_t* font14(){return &lv_font_montserrat_10;}
inline const lv_font_t* font16(){return &lv_font_montserrat_12;}
inline const lv_font_t* font18(){return &lv_font_montserrat_14;}
inline const lv_font_t* font26(){return &lv_font_montserrat_20;}
#endif
}
