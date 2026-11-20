#include "piratebox_storage.h"
#ifdef BOARD_WS_C5_273
#include <Arduino.h>
#include <LittleFS.h>
#include <esp_partition.h>
#include <esp_heap_caps.h>
#include "blank_partition.h"

bool pirateboxStorageBegin() {
  static bool attempted=false,ready=false;
  if(attempted)return ready;
  attempted=true;
  constexpr uint32_t Address=0x1000000,Bytes=0x1000000;
  const auto* partition=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_ANY,"piratefs");
  if(!partition||partition->address!=Address||partition->size!=Bytes||partition->encrypted||ESP.getFlashChipSize()<Address+Bytes) {
    Serial.println("PIRATEFS unavailable: expected unencrypted upper 16MiB partition");return false;
  }
  ready=LittleFS.begin(false,"/littlefs",8,"piratefs");
  if(ready){Serial.println("PIRATEFS mounted upper 16MiB; legacy lower filesystem preserved");return true;}
  auto* buffer=static_cast<uint8_t*>(heap_caps_malloc(4096,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
  if(!buffer){Serial.println("PIRATEFS unavailable: blank check memory; no format");return false;}
  Serial.println("PIRATEFS checking entire upper 16MiB before first-use initialization");
  const auto blank=dragon::inspectBlankRegion(Bytes,buffer,4096,
    [partition](size_t offset,uint8_t* data,size_t count){return esp_partition_read(partition,offset,data,count)==ESP_OK;},
    [](){delay(1);});
  free(buffer);
  if(blank!=dragon::BlankRegion::Blank) {
    Serial.printf("PIRATEFS preserved: %s; no format\n",blank==dragon::BlankRegion::ExistingData?"nonblank mount failure":"blank scan read failure");return false;
  }
  // Only a positively verified, fully erased partition reaches this branch.
  // Never use begin(true): a later transient mount failure must not erase data.
  Serial.println("PIRATEFS initializing verified blank upper partition");
  if(!LittleFS.format()){Serial.println("PIRATEFS initialization failed; no retry");return false;}
  ready=LittleFS.begin(false,"/littlefs",8,"piratefs");
  Serial.printf("PIRATEFS initialized=%u total=%u used=%u; legacy lower filesystem preserved\n",ready,
                unsigned(ready?LittleFS.totalBytes():0),unsigned(ready?LittleFS.usedBytes():0));
  return ready;
}
#else
bool pirateboxStorageBegin(){return false;}
#endif
