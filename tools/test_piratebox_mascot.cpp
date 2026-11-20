// Runs the actual firmware implementation against the bundled LVGL runtime.
#include <cassert>
#include <cstdio>
#include "../firmware/src/piratebox_boot.cpp"

static void flush(lv_disp_drv_t* driver,const lv_area_t*,lv_color_t*) {
  lv_disp_flush_ready(driver);
}
static void step(uint32_t ms){lv_tick_inc(ms);lv_timer_handler();}
static void pivotIs(lv_obj_t* object,int x,int y){
  lv_point_t p;lv_img_get_pivot(object,&p);assert(p.x==x&&p.y==y);
}
int main(){
  lv_init();
  static lv_color_t pixels[320*8];
  static lv_disp_draw_buf_t buffer;
  lv_disp_draw_buf_init(&buffer,pixels,nullptr,320*8);
  static lv_disp_drv_t driver;lv_disp_drv_init(&driver);
  driver.hor_res=320;driver.ver_res=320;driver.draw_buf=&buffer;driver.flush_cb=flush;
  lv_disp_drv_register(&driver);lv_img_cache_set_size(0);
  const auto animations=lv_anim_count_running();
  for(unsigned cycle=0;cycle<100;++cycle){
    auto* page=lv_obj_create(lv_scr_act());
    auto* mascot=pirateboxMascotShow(page,264,0,64);
    auto* second=pirateboxMascotShow(page,10,10,32);
    assert(mascot&&second);assert(lv_anim_count_running()==animations+2);
    assert(lv_img_get_src(mascot)==&piratebox_mascot_frames[0]);
    pirateboxMascotReact(nullptr,PirateboxMascotReaction::Wifi);
    pirateboxMascotReact(page,PirateboxMascotReaction::Wifi); // Foreign object ignored.
    lv_img_set_pivot(mascot,3,4);
    const PirateboxMascotReaction reactions[]={
      PirateboxMascotReaction::Curious,PirateboxMascotReaction::Wifi,
      PirateboxMascotReaction::Bluetooth,PirateboxMascotReaction::Camera,
      PirateboxMascotReaction::Drone,PirateboxMascotReaction::Tracker,
      PirateboxMascotReaction::Alert};
    const unsigned first[]={4,6,8,12,10,4,14};
    for(unsigned n=0;n<7;++n){
      pirateboxMascotReact(mascot,reactions[n]);
      assert(lv_img_get_src(mascot)==&piratebox_mascot_frames[first[n]]);
      step(361);
      assert(lv_img_get_src(mascot)==&piratebox_mascot_frames[first[n]+1]);
      pivotIs(mascot,3,4);pivotIs(second,0,0);
      step(2600);
      assert(lv_img_get_src(mascot)==&piratebox_mascot_frames[0]);
    }
    pirateboxMascotReact(mascot,PirateboxMascotReaction::Alert);
    lv_obj_del(page); // Deleting the parent during an active reaction.
    assert(lv_anim_count_running()==animations);
    pirateboxMascotReact(mascot,PirateboxMascotReaction::Wifi); // Deleted pointer ignored.
    step(100);
  }
  assert(pirateboxAvatarIdleFrame(3600)==pirateboxAvatarFrame(1));
  assert(pirateboxAvatarIdleFrame(6500)==pirateboxAvatarFrame(3));
  assert(pirateboxAvatarIdleFrame(8400)==pirateboxAvatarFrame(0));
  // Runtime wraps uint32_t ticks after ~49days.
  lv_tick_inc(UINT32_MAX-lv_tick_get()-100);
  lv_timer_handler(); // Establish the near-wrap clock before a new animation.
  auto* page=lv_obj_create(lv_scr_act());
  auto* mascot=pirateboxMascotShow(page,264,0,64);
  pirateboxMascotReact(mascot,PirateboxMascotReaction::Bluetooth);
  step(361);assert(lv_img_get_src(mascot)==&piratebox_mascot_frames[9]);
  step(2600);assert(lv_img_get_src(mascot)==&piratebox_mascot_frames[0]);
  lv_obj_del(page);assert(lv_anim_count_running()==animations);
  std::puts("PASS actual LVGL: 100 page lifetimes, reaction pairs/expiry, independent widgets, pivot, stale pointer, tick wrap");
}
