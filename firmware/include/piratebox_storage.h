#pragma once
// One shared LittleFS mount for badge, Personal themes, history and fallback
// recon storage. A nonblank unreadable filesystem is always preserved.
bool pirateboxStorageBegin();
