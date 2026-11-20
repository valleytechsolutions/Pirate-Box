"""Prove the actual pinned NimBLE allocator preprocesses to PSRAM only for C5."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
compiler=Path.home()/'.platformio/packages/toolchain-gccmingw32/bin/g++.exe'
core=Path(os.environ.get('PLATFORMIO_CORE_DIR',Path(os.environ.get('LOCALAPPDATA',Path.home()))/'PirateBoxSquare/platformio'))
packages=Path(os.environ.get('PLATFORMIO_PACKAGES_DIR',core/'packages'))
config=packages/'framework-arduinoespressif32-libs/esp32c5/qio_qspi/include'
libdeps=Path(os.environ.get('PLATFORMIO_LIBDEPS_DIR',ROOT/'.pio/libdeps'))
nimble=libdeps/'waveshare_c5_273/NimBLE-Arduino/src'
source=nimble/'nimble/esp_port/port/src/exp_nimble_mem.c'
env=dict(os.environ);env['PATH']=str(compiler.parent)+os.pathsep+env.get('PATH','')
with tempfile.TemporaryDirectory(prefix='piratebox-memory-') as directory:
    stub=Path(directory)
    (stub/'esp_attr.h').write_text('')
    (stub/'esp_heap_caps.h').write_text('// Capability tokens deliberately remain visible in preprocessed output.\n')
    command=[str(compiler),'-E','-P','-DESP_PLATFORM','-I'+str(stub),'-I'+str(config),'-I'+str(nimble),'-I'+str(ROOT/'firmware/include')]
    baseline=subprocess.check_output(command+[str(source)],env=env,text=True)
    external=subprocess.check_output(command+['-DBOARD_WS_C5_273','-include','piratebox_c5_nimble_memory.h',str(source)],env=env,text=True)
    assert 'return heap_caps_malloc(size, MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);' in baseline
    assert 'return heap_caps_malloc(size, MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);' in external
    assert 'return heap_caps_calloc(n, size, MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);' in external
    assert 'return heap_caps_malloc(size, MALLOC_CAP_INTERNAL' not in external
    exe=stub/'storage-test.exe'
    subprocess.run([str(compiler),'-std=c++17','-Wall','-Wextra','-Werror','-Ifirmware/include','tools/test_piratebox_storage.cpp','-o',str(exe)],cwd=ROOT,env=env,check=True)
    subprocess.run([str(exe)],env=env,check=True)
print('C5 NimBLE malloc/calloc select PSRAM; unchanged default selects internal RAM. Actual pinned allocator/SDK preprocessing PASS.')
