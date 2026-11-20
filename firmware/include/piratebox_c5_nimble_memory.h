#pragma once
// Force-included only by the C5 profile. The Arduino SDK defaults to INTERNAL,
// and NimBLE checks that macro before EXTERNAL, so a command-line -D alone is
// ineffective. Load the SDK first, then select NimBLE's supported PSRAM path.
#include "sdkconfig.h"
#if defined(BOARD_WS_C5_273)
#undef CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_INTERNAL
#undef CONFIG_NIMBLE_MEM_ALLOC_MODE_INTERNAL
#define CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL 1
#define CONFIG_NIMBLE_MEM_ALLOC_MODE_EXTERNAL 1
#endif
