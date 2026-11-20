#pragma once
#ifdef BOARD_WS_C5_273
#include <Arduino.h>
#include <esp_heap_caps.h>
namespace piratebox {
[[noreturn]] inline void memoryStartupFailed(const char* table){
  Serial.printf("FATAL: PSRAM allocation failed for %s; startup stopped\n",table);
  // Yield to USB/RTOS tasks, without entering application code with null tables.
  while(true)delay(100);
}
inline void heapStatus(const char* stage){
  constexpr unsigned caps=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
  Serial.printf("C5_HEAP stage=%s internal_free=%u internal_largest=%u internal_min=%u psram_free=%u\n",
    stage,unsigned(heap_caps_get_free_size(caps)),unsigned(heap_caps_get_largest_free_block(caps)),
    unsigned(heap_caps_get_minimum_free_size(caps)),ESP.getFreePsram());
}
}
#endif
