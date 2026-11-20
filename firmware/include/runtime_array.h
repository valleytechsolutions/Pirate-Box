#pragma once
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <new>
#include <type_traits>

namespace piratebox {
// Runtime storage for application tables. Constructors run only after the
// caller supplies an initialized allocator; no allocation during static init.
template<class T, size_t Count> class RuntimeArray {
  static_assert(Count > 0 && Count <= std::numeric_limits<size_t>::max()/sizeof(T), "Invalid table size");
  static_assert(std::is_nothrow_default_constructible<T>::value, "Table constructors must not throw");
  T* values_=nullptr;
public:
  RuntimeArray()=default;
  RuntimeArray(const RuntimeArray&)=delete;
  RuntimeArray& operator=(const RuntimeArray&)=delete;
  ~RuntimeArray(){if(values_){for(size_t i=0;i<Count;++i)values_[i].~T();std::free(values_);}}
  bool begin(void* (*allocate)(size_t)) {
    if(values_)return true;
    if(!allocate)return false;
    auto* memory=static_cast<T*>(allocate(sizeof(T)*Count));
    if(!memory)return false;
    for(size_t i=0;i<Count;++i)new(memory+i) T{};
    values_=memory;
    return true;
  }
  constexpr size_t size()const{return Count;}
  bool ready()const{return values_!=nullptr;}
  T* get(size_t index){return values_&&index<Count?values_+index:nullptr;}
  const T* get(size_t index)const{return values_&&index<Count?values_+index:nullptr;}
  T& operator[](size_t index){assert(values_&&index<Count);return values_[index];}
  const T& operator[](size_t index)const{assert(values_&&index<Count);return values_[index];}
};
}
