"""Production ESP32-C5 public CSI adapter host tests; no serial device access."""
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[1]
work=root/'.work/csi-tests'
fake=work/'fake-include'
fake.mkdir(parents=True,exist_ok=True)
for name in ('csi_monitor.h','app.h','ble_lab.h','recon_internal.h','esp_wifi.h'):
    (fake/name).write_text('#pragma once\n#include "csi_c5_host_stub.h"\n')
(fake/'esp_idf_version.h').write_text('#pragma once\n#define ESP_IDF_VERSION_VAL(a,b,c) (((a)<<16)|((b)<<8)|(c))\n#define ESP_IDF_VERSION ESP_IDF_VERSION_VAL(5,5,5)\n')
exe=work/('csi-test.exe' if sys.platform=='win32' else 'csi-test')
subprocess.run([sys.executable,'-m','ziglang','c++','-UNDEBUG','-std=c++17','-O1',
               '-I'+str(root/'tests'),'-I'+str(fake),'-I'+str(root/'firmware/include'),
               str(root/'tests/csi_c5_test.cpp'),'-o',str(exe)],check=True,cwd=root)
subprocess.run([str(exe)],check=True,cwd=root)
