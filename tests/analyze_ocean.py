#!/usr/bin/env python3
from pathlib import Path
import array,hashlib,json,math,subprocess,sys,wave
import argparse
p=argparse.ArgumentParser(description="Analyze a deterministic 60-second generated ocean fixture")
p.add_argument('render',type=Path)
p.add_argument('--output',type=Path,required=True)
args=p.parse_args();OUT=args.output;OUT.mkdir(parents=True,exist_ok=True)
data=subprocess.check_output([str(args.render),'ocean','440','60000','20','24000']);a=array.array('h');a.frombytes(data);assert len(a)==1440000
if sys.byteorder!='little':a.byteswap()
preview=OUT/'ocean-surf-60s-24k.wav'
with wave.open(str(preview),'wb') as wav:wav.setnchannels(1);wav.setsampwidth(2);wav.setframerate(24000);wav.writeframes(a.tobytes())
def stats(start,n=12000):
 x=a[start:start+n];power=sum(v*v for v in x)/len(x);return {'rms':math.sqrt(power),'difference_power_ratio':sum((x[i]-x[i-1])**2 for i in range(1,len(x)))/(len(x)-1)/power}
rng=12345^0x9e3779b9;offset=0;cycles=[];last=0
while offset<len(a):
 rng^=(rng<<13)&0xffffffff;rng^=rng>>17;rng^=(rng<<5)&0xffffffff;rng&=0xffffffff
 count=24000*(8000+rng%7001)//1000
 if count==last:count=24000*8 if count==24000*15 else count+24
 last=count
 if offset+count>len(a):break
 retreat=stats(offset+2400);crest=stats(offset+count//2-6000)
 assert crest['rms']>retreat['rms']*3,(retreat,crest)
 assert crest['difference_power_ratio']>retreat['difference_power_ratio']*2,(retreat,crest)
 cycles.append({'start_sample':offset,'samples':count,'seconds':count/24000,'retreat':retreat,'crest':crest})
 offset+=count
assert len(cycles)>=4 and len({c['samples'] for c in cycles})==len(cycles)
result={'samples':len(a),'sample_rate':24000,'preview_explicit_level_percent':20,'seed':12345,'peak_counts':max(map(abs,a)),'mean_counts':sum(a)/len(a),'rms_counts':math.sqrt(sum(v*v for v in a)/len(a)),'complete_cycles':cycles,'preview_sha256':hashlib.sha256(preview.read_bytes()).hexdigest()}
(OUT/'ocean-analysis.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
