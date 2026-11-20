"""Host regression checks for the production Pirate Box detection feed."""
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[1]
out=root/'.work/detection-feed-test'
out.parent.mkdir(exist_ok=True)
if sys.platform=='win32':out=out.with_suffix('.exe')
subprocess.run([sys.executable,'-m','ziglang','c++','-UNDEBUG','-std=c++17','-O1','-I'+str(root/'firmware/include'),'-I'+str(root/'lib/opendroneid/src'),str(root/'tests/detection_feed_test.cpp'),'-o',str(out)],check=True,cwd=root)
subprocess.run([str(out)],check=True,cwd=root)
