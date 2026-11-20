"""Verify production C5 theme/clock correspondence without serial access."""
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[1]
out=root/'.work/pirate-clock-mapping-test'
if sys.platform=='win32':out=out.with_suffix('.exe')
out.parent.mkdir(exist_ok=True)
subprocess.run([sys.executable,'-m','ziglang','c++','-UNDEBUG','-std=c++17','-O1','-I'+str(root/'firmware/include'),str(root/'tests/pirate_clock_mapping_test.cpp'),'-o',str(out)],check=True,cwd=root)
subprocess.run([str(out)],check=True,cwd=root)
