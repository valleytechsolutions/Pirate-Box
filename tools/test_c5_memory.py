"""C5 PSRAM tables: actual table constructors, allocation failure, bounds/lifecycle."""
from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
work = root / '.work/c5-memory-tests'
work.mkdir(parents=True, exist_ok=True)
app = (root / 'firmware/include/app.h').read_text()
ui = (root / 'firmware/src/ui.cpp').read_text()
main = (root / 'firmware/src/main.cpp').read_text()
alert = re.search(r'struct Alert \{[^\n]+', app).group()
dot = re.search(r'struct Dot \{[^\n]+', ui).group()
assert main.index('if(!storageMemoryBegin())') < main.index('runSelfTests();boardBegin();')
assert main.index('if(!bleLabMemoryBegin())') < main.index('runSelfTests();boardBegin();')
assert ui.index('if(!dots.begin(ps_malloc))') < ui.index('if(!prepareThemeLogos())')
source = work / 'memory.cpp'
source.write_text(r'''
#include "runtime_array.h"
#include "beeper_session.h"
#include <cstring>
#include <cstdio>
#include <cassert>
''' + alert + '\n' + dot + r'''
unsigned calls=0,live=0,constructed=0;
void* fail(size_t){++calls;return nullptr;}
void* dirty(size_t size){++calls;auto* p=std::malloc(size);assert(p);std::memset(p,0xA5,size);return p;}
struct Counted {
  int value=42;
  Counted()noexcept{++live;++constructed;}
  ~Counted(){--live;}
};
int main(){
  using piratebox::RuntimeArray;
  {
    RuntimeArray<Counted,4> table;
    assert(!table.ready()&&!table.get(0)&&!table.get(4)&&table.size()==4);
    assert(!table.begin(nullptr)&&!table.begin(fail)&&calls==1&&constructed==0&&live==0);
    assert(table.begin(dirty)&&table.ready()&&calls==2&&constructed==4&&live==4);
    for(size_t i=0;i<4;++i)assert(table[i].value==42);
    table[3].value=91;
    assert(!table.get(4)&&!table.get(size_t(-1)));
    assert(table.begin(fail)&&calls==2&&table[3].value==91); // No reallocation/reset.
    const auto& view=table;assert(view.get(3)->value==91&&!view.get(4)&&view[0].value==42);
  }
  assert(live==0);
  RuntimeArray<Alert,40> alerts;
  RuntimeArray<Dot,384> dots;
  RuntimeArray<dragon::tracker::BeeperSession<384>,1> beeps;
  assert(alerts.begin(dirty)&&dots.begin(dirty)&&beeps.begin(dirty));
  for(size_t i=0;i<alerts.size();++i){auto& a=alerts[i];assert(a.at==0&&a.title[0]==0&&a.detail[127]==0&&a.mac[17]==0);}
  for(size_t i=0;i<dots.size();++i){auto& d=dots[i];assert(d.x==0&&d.y==0&&d.index==0&&d.key.mac[5]==0&&d.labelSlot==-1);}
  assert(!beeps[0].active&&beeps[0].count==0&&beeps[0].current==-1&&beeps[0].last==-1);
  for(auto& t:beeps[0].targets)assert(t.attempts==0&&!t.disabled&&t.result==dragon::tracker::AttemptResult::Waiting);
  dragon::DeviceKey key{};
  for(unsigned i=0;i<384;++i){key.mac[0]=i;key.mac[1]=i>>8;assert(beeps[0].add(key,1234));}
  key.mac[2]=1;assert(!beeps[0].add(key,1234)); // Capacity stays unchanged.
  assert(beeps[0].begin()&&beeps[0].count==384);beeps[0].reset(false,4321);
  assert(!beeps[0].active&&beeps[0].count==0&&beeps[0].current==-1&&beeps[0].last==-1);
  puts("PASS C5 PSRAM tables: dirty-memory construction, constructor lifetime, failed allocation/retry, idempotent startup, first/last entries, checked bounds, complete384-target beeper capacity");
}
''')
exe = work / ('memory.exe' if sys.platform == 'win32' else 'memory')
subprocess.run([sys.executable, '-m', 'ziglang', 'c++', '-UNDEBUG', '-std=c++17', '-O1',
                '-I'+str(root/'firmware/include'), '-I'+str(root/'lib/opendroneid/src'),
                str(source), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
