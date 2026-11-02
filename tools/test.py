#!/usr/bin/env python3
"""Run focused host/installed-source checks using existing tools; no downloads."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--build-dir',type=Path,required=True)
p.add_argument('--checkout',type=Path,help='Installed pinned consumer for actual-source checks')
p.add_argument('--otto',type=Path,help='Existing public animated-faces source, for installer-order checks')
a=p.parse_args();out=a.build_dir.resolve()
if out==ROOT or ROOT in out.parents:
    raise SystemExit('Use a reusable validation directory outside this source package')
for tool in ['cmake','ctest','c++','cc','git']:
    if not shutil.which(tool):raise SystemExit('Required existing tool is missing: '+tool)
def run(*cmd):subprocess.run([str(x) for x in cmd],check=True)
run('cmake','-S',ROOT,'-B',out,'-DCMAKE_BUILD_TYPE=Debug',
    '-DCMAKE_CXX_FLAGS=-Wall -Wextra -Werror -fsanitize=undefined,float-cast-overflow -fno-sanitize-recover=all')
run('cmake','--build',out,'-j4')
run('ctest','--test-dir',out,'--output-on-failure')
run(sys.executable,ROOT/'tests/check_level_edges.py')
if a.checkout:
    run(sys.executable,ROOT/'tests/check_shared_playback.py',a.checkout,'--output',out.parent/'integration-tests')
    run(sys.executable,ROOT/'tests/check_callbacks.py',a.checkout,'--output',out.parent/'callback-tests')
    command=[sys.executable,ROOT/'tests/check_installer.py',a.checkout,'--output',out.parent/'installer-fixture']
    if a.otto:command+=['--otto',a.otto]
    run(*command)
