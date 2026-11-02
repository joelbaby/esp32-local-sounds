#!/usr/bin/env python3
"""Exercise install/remove/conflict transactions in one small reusable source fixture."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
ROOT=Path(__file__).resolve().parents[1]
PIN='addb1ed59bd71e09a6a5e0c52ad2f1c474aa8ca1'
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('upstream',type=Path)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--otto',type=Path,help='Optional existing public animated-faces source package')
a=p.parse_args();target=a.output.resolve();target.mkdir(parents=True,exist_ok=True)
records=json.loads((ROOT/'integrations/xiaozhi/patch-files.json').read_text())
files=set().union(*(set(r) for r in records.values()))|{'main/Kconfig.projbuild','main/display/oled_display.cc','main/display/oled_display.h'}
# The fixture contains only touched source files, not another firmware build/SDK.
def run(*cmd,okay=True):
 result=subprocess.run([str(x) for x in cmd],capture_output=True,text=True)
 if okay and result.returncode:raise RuntimeError(result.stdout+result.stderr)
 if not okay:assert result.returncode,(cmd,result.stdout)
 return result
if (target/'.git').exists():
 if not (target/'.local-sounds-test-fixture').is_file():
  raise SystemExit('Refusing to reuse an unmarked directory')
 shutil.rmtree(target);target.mkdir()
(target/'.local-sounds-test-fixture').write_text('Disposable source-only installer fixture\n')
run('git','-C',target,'init','-q')
objects=Path(run('git','-C',a.upstream,'rev-parse','--git-path','objects').stdout.strip())
if not objects.is_absolute():objects=a.upstream/objects
(target/'.git/objects/info/alternates').write_text(str(objects.resolve())+'\n')
run('git','-C',target,'update-ref','HEAD',PIN)
for name in files:
 data=subprocess.check_output(['git','-C',str(a.upstream),'show',PIN+':'+name])
 (target/name).parent.mkdir(parents=True,exist_ok=True);(target/name).write_bytes(data)
base={n:(target/n).read_bytes() for n in files}
(target/'unrelated.txt').write_text('Keep my independent source work.\n')
def command(*args,okay=True):return run('python3',ROOT/'tools/install_xiaozhi.py',target,*args,okay=okay)
def snapshot():return {str(f.relative_to(target)):hashlib.sha256(f.read_bytes()).hexdigest() for f in target.rglob('*') if f.is_file() and '.git' not in f.parts}
command();installed=snapshot()
command(okay=False);assert snapshot()==installed
# Conflicting removal preserves every byte.
f=target/'main/application.cc';f.write_text(f.read_text()+'\n// consumer edit\n')
conflict=snapshot();command('--remove',okay=False);assert snapshot()==conflict
f.write_bytes(base['main/application.cc']) # restore installed from exact patch instead below
# Return to installed source without resetting any other fixture file.
patch=ROOT/'integrations/xiaozhi/shared-playback.patch'
for part in ['shared-playback','sounds']:
 # Reapply only Application hunks using include filter.
 run('git','-C',target,'apply','--include=main/application.cc',ROOT/f'integrations/xiaozhi/{part}.patch')
command('--remove');assert all((target/n).read_bytes()==b for n,b in base.items())
assert (target/'unrelated.txt').read_text()=='Keep my independent source work.\n'
# A dirty shared source refuses installation before any mutation.
f=target/'main/audio/audio_service.h';f.write_bytes(base['main/audio/audio_service.h']+b'// custom\n')
conflict=snapshot();command(okay=False);assert snapshot()==conflict
f.write_bytes(base['main/audio/audio_service.h'])
# Standalone shared session glue installs without generator source/includes.
command('--part','shared-playback')
assert not (target/'main/audio/tone_noise_generator.h').exists()
assert 'tone_noise' not in (target/'main/application.h').read_text()
assert 'tone_noise' not in (target/'main/audio/audio_service.h').read_text()
command('--part','sounds');command('--part','sounds','--remove');command('--part','shared-playback','--remove')
assert all((target/n).read_bytes()==b for n,b in base.items())
# Optional hardware board is an explicit separate transaction.
run('python3',ROOT/'tools/install_example_board.py',target)
command();command('--remove')
run('python3',ROOT/'tools/install_example_board.py',target,'--remove')
assert all((target/n).read_bytes()==b for n,b in base.items())
if a.otto:
 # Sound first -> old Otto installer safely refuses -> sound remove -> Otto
 # install -> sounds reinstall/remove. The Otto repository is never changed.
 command();before=snapshot()
 run('python3',a.otto/'tools/apply_xiaozhi.py',target,okay=False);assert snapshot()==before
 command('--remove')
 run('python3',a.otto/'tools/apply_xiaozhi.py',target)
 otto=snapshot();command();command('--remove');assert snapshot()==otto
 print('Otto: safe refusal in sound-first order; documented remove/Otto/reinstall workflow and Otto-first sound roundtrip passed.')
print('Installer: two roundtrips, idempotence refusal, dirty-source/removal byte preservation, shared-only independence and optional board passed.')
