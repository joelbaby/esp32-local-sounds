#!/usr/bin/env python3
"""Deterministic60-second pink20% Welch fixture; standard library only."""
import array
import cmath
import json
import math
import subprocess
import sys

samples = array.array('h', subprocess.check_output([sys.argv[1], 'pink', '440', '60000', '20', '24000']))
if sys.byteorder != 'little':
    # render_pcm emits native-endian PCM; analysis runs on the same host.
    pass
assert len(samples) == 1440000
rate, size = 24000, 4096
window = [0.5-0.5*math.cos(2*math.pi*i/(size-1)) for i in range(size)]
# Fixed input seed 12345, omit startup/end ramps and separate frames by 8192.
reverse = [int(f'{i:012b}'[::-1], 2) for i in range(size)]
twiddles = {}
length = 2
while length <= size:
    twiddles[length] = [cmath.exp(-2j*math.pi*k/length) for k in range(length//2)]
    length *= 2
power = [0.]*(size//2+1)
frames = 0
for start in range(12000, len(samples)-size-12000, 8192):
    data = [complex(samples[start+reverse[i]]*window[reverse[i]]) for i in range(size)]
    length = 2
    while length <= size:
        half = length//2
        twiddle = twiddles[length]
        for base in range(0, size, length):
            for k in range(half):
                a = data[base+k]
                b = data[base+k+half]*twiddle[k]
                data[base+k], data[base+k+half] = a+b, a-b
        length *= 2
    for k in range(len(power)):
        power[k] += abs(data[k])**2
    frames += 1
bands = []
lo = 100.
while lo < 4000:
    hi = min(lo*2**0.5, 4000.)
    bins = [k for k in range(1,len(power)) if lo <= k*rate/size < hi]
    density = sum(power[k] for k in bins)/len(bins)/frames
    bands.append((math.sqrt(lo*hi), density))
    lo = hi
xs = [math.log10(f) for f,p in bands]
ys = [10*math.log10(p) for f,p in bands]
xmean,ymean=sum(xs)/len(xs),sum(ys)/len(ys)
slope = sum((x-xmean)*(y-ymean) for x,y in zip(xs,ys))/sum((x-xmean)**2 for x in xs)
rms = math.sqrt(sum(float(x)*x for x in samples)/len(samples))/32768
mean = sum(samples)/len(samples)/32768
peak = max(abs(x) for x in samples)/32768
assert -12 < slope < -8, slope
assert abs(mean) < rms*0.05, (mean,rms)
assert 0.025 < rms < 0.045, rms
assert peak <= 0.2, peak
print(json.dumps({'samples':len(samples),'seed':12345,'frames':frames,
                 'mean_full_scale':mean,'rms_full_scale':rms,'peak_full_scale':peak,
                 'slope_db_per_decade_100_4000_hz':slope,
                 'bands_relative_db':[[round(f,2),round(10*math.log10(p/bands[0][1]),3)] for f,p in bands]},indent=2))
