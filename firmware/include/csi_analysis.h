#pragma once
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <algorithm>
namespace dragon::csi {
// LLTF only: fixed, bounded layouts. Ignore the first two IQ pairs
// on every packet, including packets whose first word is valid.
template<size_t Pairs> struct ShapeFor {
 static_assert(Pairs>2 && Pairs<=64,"Supported LLTF layout must have 3..64 complex bins");
 float previous[Pairs-2]{};bool ready=false;
 float observe(const int8_t* iq,size_t length) {
  if(!iq||length!=Pairs*2)return -1;
  float current[Pairs-2],energy=0;
  for(unsigned i=0;i<Pairs-2;++i){float a=iq[4+i*2],b=iq[5+i*2];current[i]=std::sqrt(a*a+b*b);energy+=current[i]*current[i];}
  if(energy<1)return -1;
  float scale=1/std::sqrt(energy),dot=0;
  for(unsigned i=0;i<Pairs-2;++i){current[i]*=scale;dot+=previous[i]*current[i];previous[i]=current[i];}
  if(!ready){ready=true;return -1;}
  return std::max(0.0f,std::min(1.0f,1-dot));
 }
};
// Keep the original S3 type and 128-byte behavior unchanged.
using Shape=ShapeFor<64>;
struct Baseline {
 unsigned count=0;float mean=0,m2=0;
 void add(float v){if(!std::isfinite(v)||v<0)return;++count;float d=v-mean;mean+=d/count;m2+=d*(v-mean);}
 float threshold()const{return mean+std::max(.004f,3*std::sqrt(count>1?m2/(count-1):0));}
 unsigned score(float v)const {if(!count||!std::isfinite(v)||v<=mean)return 0;return std::min(100u,unsigned(std::min(1.0f,(v-mean)/std::max(.008f,2*(threshold()-mean)))*100));}
};
}
