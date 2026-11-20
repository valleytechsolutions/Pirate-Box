"""Compile and run the actual C5 mascot implementation with the bundled LVGL.
No board or serial connection. Uses the local MinGW host compiler.
"""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import os, subprocess, hashlib

ROOT=Path(__file__).resolve().parents[1]
LVGL=ROOT/'hardware/spotpear/lib/lvgl'
BUILD=ROOT/'.work/mascot-lvgl-test'
BIN=Path.home()/'.platformio/packages/toolchain-gccmingw32/bin'
FLAGS=['-DLV_CONF_SKIP','-DLV_COLOR_DEPTH=16','-DLV_COLOR_16_SWAP=1',
       '-DLV_MEM_SIZE=262144','-DBOARD_WS_C5_273','-O0',
       '-I'+str(LVGL),'-I'+str(ROOT/'firmware/include')]
ENV=dict(os.environ)
ENV['PATH']=str(BIN)+os.pathsep+ENV.get('PATH','')
def run(args):
    result=subprocess.run([str(a) for a in args],env=ENV,capture_output=True,text=True)
    if result.returncode:raise RuntimeError(result.stdout+result.stderr)
def compile_c(source):
    obj=BUILD/(hashlib.sha256(str(source).encode()).hexdigest()[:16]+'.o')
    if not obj.exists() or obj.stat().st_mtime<source.stat().st_mtime:
        run([BIN/'gcc.exe',*FLAGS,'-std=c99','-c',source,'-o',obj])
    return obj
def main():
    BUILD.mkdir(parents=True,exist_ok=True)
    sources=list((LVGL/'src').rglob('*.c'))
    with ThreadPoolExecutor(max_workers=4) as pool:objects=list(pool.map(compile_c,sources))
    archive=BUILD/'lvgl.a'
    run([BIN/'ar.exe','rcs',archive,*objects])
    exe=BUILD/'mascot.exe'
    run([BIN/'g++.exe',*FLAGS,'-std=c++17',ROOT/'tools/test_piratebox_mascot.cpp',archive,'-o',exe])
    result=subprocess.run([str(exe)],env=ENV,text=True,capture_output=True)
    print(result.stdout,end='')
    if result.returncode:raise RuntimeError(result.stderr or 'Mascot runtime failed '+str(result.returncode))
if __name__=='__main__':main()
