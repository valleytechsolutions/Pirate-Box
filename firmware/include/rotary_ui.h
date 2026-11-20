#pragma once
#if defined(BOARD_CROWPANEL_21) || defined(BOARD_WATCHER) || defined(BOARD_WS_C5_273)
#include <lvgl.h>
void rotaryUiBegin();
void rotaryUiLoop();
void rotaryUiInput(int steps,bool click,bool back);
#ifdef BOARD_WS_C5_273
// Opt in one live text viewport: BOOT reads, L/R scroll, BOOT/hold exits.
// Reset or deleting the viewport removes its registration.
void rotaryUiScrollRegion(lv_obj_t* object);
#endif
lv_obj_t* uiRotaryScope();
bool uiRotaryWake();
#ifdef BOARD_WATCHER
bool uiRotaryOpenMenu();
void rotaryUiTouch(bool down);
#endif
#if defined(BOARD_WATCHER) || defined(BOARD_WS_C5_273)
void rotaryUiReset();
#endif
#endif
