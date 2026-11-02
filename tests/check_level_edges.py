#!/usr/bin/env python3
"""Compile the actual PCM gain expression at representable signed16 endpoints.

Public stream/gain tests cover every mode; this additional boundary check reaches
an all-negative pink-sum edge that a finite pseudo-random sequence need not visit.
"""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'src/tone_noise_generator.cc').read_text()
start=source.index('        output[count++] = static_cast<int16_t>(')
statement=source[start:source.index(';',start)+1]
fixture='''#include "tone_noise_generator.h"
#include <cassert>
using namespace tone_noise;
struct GainProbe {Parameters parameters_;uint32_t ramp_=120;
int16_t apply(int32_t value,uint32_t envelope){int16_t output[1];size_t count=0;
'''+statement+'''
return output[0];}};
int main(){GainProbe gain;
assert(gain.apply(-32768,120)==-32768);assert(gain.apply(32767,120)==32767);
assert(gain.apply(-32768,60)==-16384);assert(gain.apply(-32768,0)==0);
gain.parameters_.level_percent=20;
assert(gain.apply(-32768,120)==-6553);assert(gain.apply(32767,120)==6553);}
'''
with tempfile.TemporaryDirectory(prefix='pcm-level-edges-') as directory:
    path=Path(directory);(path/'edge.cc').write_text(fixture)
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(root/'include'),str(path/'edge.cc'),'-o',str(path/'edge')],check=True)
    subprocess.run([str(path/'edge')],check=True)
print('Actual PCM gain expression preserves -32768/+32767 at100 and attenuation at20, without wrap.')
