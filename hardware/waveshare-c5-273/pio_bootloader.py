"""Use the checked-in C5 bootloader image for the pinned Arduino core.

The image was generated from Arduino 3.3.12 / IDF 5.5.5's
bootloader_qio_80m.elf with a DIO, 80 MHz, 32 MB image header. The C5 ROM
must load the second-stage bootloader in DIO; that bootloader then enables
QIO for the application. A QIO ROM image header causes an invalid-image
reset loop even when every flashed byte verifies. Keep the ELF, header,
app flash geometry and this image in sync when updating the core.
"""

from pathlib import Path

Import("env")

bootloader = Path(env.subst("$PROJECT_DIR")) / "hardware" / "waveshare-c5-273" / "bootloader.bin"
env.BoardConfig().update("build.arduino.custom_bootloader", str(bootloader))
