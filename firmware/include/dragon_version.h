#pragma once
// Release packager reads this same value for all board manifests.
#define DRAGON_VERSION "0.13.42"
#ifdef BOARD_WS_C5_273
#define FIRMWARE_NAME "PIRATE BOX"
#define FIRMWARE_VERSION "0.1.0"
#else
#define FIRMWARE_NAME "DRAGON DETECTOR"
#define FIRMWARE_VERSION DRAGON_VERSION
#endif
