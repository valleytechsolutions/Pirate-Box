#pragma once
#ifdef BOARD_WS_C5_273
#include <FS.h>

// Arduino's File API over the IDF FAT VFS. The native SD and LCD drivers share
// SPI2 through the IDF bus arbiter; no second Arduino SPI owner is created.
class C5CardFileSystem : public fs::FS {
public:
  C5CardFileSystem();
  void attach(uint64_t capacity);
  uint64_t cardSize() const;
  uint64_t totalBytes();
  uint64_t usedBytes();
private:
  uint64_t capacity_=0;
};
extern C5CardFileSystem C5CardFS;
#endif
