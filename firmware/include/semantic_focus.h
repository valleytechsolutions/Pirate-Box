#pragma once
#include <lvgl.h>
#include <cstring>

namespace piratebox {
// Bounded, caller-owned storage. Capture before deleting the old widgets and
// register exact semantic keys while building their replacements. No row-index
// or address-reuse fallback is allowed. Ambiguous keys deliberately do not focus.
template<unsigned Capacity=160,unsigned KeySize=160> struct SemanticFocus {
 struct Binding {lv_obj_t* object=nullptr;char key[KeySize]{};};
 Binding bindings[Capacity]{};unsigned count=0,matches=0;
 char captured[KeySize]{};lv_obj_t* replacement=nullptr;bool editing=false;
 lv_obj_t* begin(bool preserve){
   captured[0]=0;replacement=nullptr;matches=0;editing=false;lv_obj_t* focused=nullptr;
   if(preserve)for(unsigned i=0;i<count;++i)if(bindings[i].key[0]&&lv_obj_has_state(bindings[i].object,LV_STATE_FOCUSED)){
     focused=bindings[i].object;std::memcpy(captured,bindings[i].key,KeySize);
     auto* group=static_cast<lv_group_t*>(lv_obj_get_group(focused));editing=group&&lv_group_get_editing(group);break;
   }
   count=0;return focused;
 }
 bool capturedIs(const char* key)const{return captured[0]&&key&&key[0]&&!std::strcmp(captured,key);}
 void bind(lv_obj_t* object,const char* key){
   if(!object||!key||!key[0]||std::strlen(key)>=KeySize)return;
   if(count<Capacity){auto& item=bindings[count++];item.object=object;std::strcpy(item.key,key);}
   if(capturedIs(key)){replacement=object;++matches;}
 }
 bool restore(){
   if(matches!=1||!replacement||lv_obj_has_flag(replacement,LV_OBJ_FLAG_HIDDEN)||lv_obj_has_state(replacement,LV_STATE_DISABLED))return false;
   auto* group=static_cast<lv_group_t*>(lv_obj_get_group(replacement));if(!group)return false;
   lv_group_focus_obj(replacement);lv_group_set_editing(group,editing);return true;
 }
};
}
