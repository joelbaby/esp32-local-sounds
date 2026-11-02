#!/usr/bin/env python3
"""Observe explicit20% PCM fixture: detune, harmonic guard, score timing and level."""
import array
import cmath
import json
import math
import subprocess
import sys
import wave
from pathlib import Path
render = sys.argv[1]
def pcm(kind, frequency=440, duration=5000, rate=24000, level=20):
    return array.array('h', subprocess.check_output([render,kind,str(frequency),str(duration),str(level),str(rate)]))
def spectrum(samples, rate, start=1.):
    size = 65536 if rate == 24000 else 32768
    offset = int(start*rate)
    assert offset+size <= len(samples)
    bits = int(math.log2(size))
    reverse = [int(f'{i:0{bits}b}'[::-1],2) for i in range(size)]
    window = [0.5-0.5*math.cos(2*math.pi*i/(size-1)) for i in range(size)]
    data = [complex(samples[offset+reverse[i]]*window[reverse[i]]) for i in range(size)]
    length=2
    while length <= size:
        half=length//2
        twiddle=[cmath.exp(-2j*math.pi*k/length) for k in range(half)]
        for base in range(0,size,length):
            for k in range(half):
                a=data[base+k];b=data[base+k+half]*twiddle[k]
                data[base+k],data[base+k+half]=a+b,a-b
        length*=2
    return [abs(x)**2 for x in data[:size//2+1]], rate/size
def band(power, spacing, low, high):
    return sum(power[k] for k in range(max(1,math.ceil(low/spacing)),min(len(power),math.floor(high/spacing)+1)))
def stats(samples):
    mean=sum(samples)/len(samples)
    rms=math.sqrt(sum(float(x)*x for x in samples)/len(samples))
    return {'samples':len(samples),'mean_counts':mean,'rms_counts':rms,'peak_counts':max(abs(x) for x in samples)}
base=pcm('brass')
power,spacing=spectrum(base,24000)
peaks=[]
for low,high in [(438.4,439.7),(440.3,441.5)]:
    indices=range(math.ceil(low/spacing),math.floor(high/spacing)+1)
    peaks.append(max(indices,key=lambda k:power[k])*spacing)
expected=[440*(1-1/512),440*(1+1/512)]
assert all(abs(a-b)<0.5 for a,b in zip(peaks,expected)),(peaks,expected)
assert band(power,spacing,875,885)/band(power,spacing,437,443)>0.01
b=stats(base);assert b['peak_counts']<=6553
assert abs(b['mean_counts']) < b['rms_counts']*0.05
# At1700/24k, selected four harmonics end below6814Hz; at2000/16k,
# selected two end below4008Hz. Observe the absent high/alias region in output.
guards=[]
for frequency,rate,high_start in [(1700,24000,7300),(2000,16000,4500)]:
    samples=pcm('brass',frequency,5000,rate)
    powers,df=spectrum(samples,rate)
    ratio=band(powers,df,high_start,rate/2)/band(powers,df,frequency-10,frequency+10)
    assert ratio<1e-4,ratio
    guards.append({'frequency_hz':frequency,'rate':rate,'guard_region_from_hz':high_start,'guard_power_over_fundamental':ratio})
tune=pcm('axel_f',440,0)
t=stats(tune);assert t['samples']==160000 and t['peak_counts']<=6553
assert abs(t['mean_counts']) < t['rms_counts']*0.05
# Known independent onsets from the supplied23-event source; observe pitch by
# correlation with a sine across60ms of each selected note (fundamental dominates).
notes=[]
onset_ticks=[0,4,7,9,10,12,14,16,20,23,25,26,28,30,32,34,36,38,39,41,42,44,46]
expected_pitches=[349,415,349,349,466,349,311,349,523,349,349,554,523,415,349,523,698,349,311,311,261,392,349]
for tick,frequency in zip(onset_ticks,expected_pitches):
    start=24000*60*tick//(117*4)+15*24
    count=60*24
    scores={f:abs(sum(tune[start+i]*cmath.exp(-2j*math.pi*f*i/24000) for i in range(count))) for f in range(200,901)}
    measured=max(scores,key=scores.get)
    assert abs(measured-frequency)<=4,(tick,frequency,measured)
    notes.append({'onset_tick':tick,'expected_hz':frequency,'measured_hz':measured})
result={'brass':b,'fundamental_peak_bins_hz':peaks,'expected_detuned_hz':expected,
        'harmonic_guard_checks':guards,'axel_f':t,'selected_notes':notes}
if len(sys.argv)>2:
    destination=Path(sys.argv[2]);destination.parent.mkdir(parents=True,exist_ok=True)
    preview=pcm('axel_f',440,0,24000,5)
    if sys.byteorder!='little':preview.byteswap()
    with wave.open(str(destination),'wb') as wav:
        wav.setnchannels(1);wav.setsampwidth(2);wav.setframerate(24000);wav.writeframes(preview.tobytes())
    result['preview_level_percent']=5
print(json.dumps(result,indent=2))
