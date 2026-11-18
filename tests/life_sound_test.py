# SPDX-License-Identifier: GPL-3.0-only
"""Equal-RMS spectral checks: hiss cannot pass merely by becoming quieter."""
import json, wave
from pathlib import Path
import numpy as np
ROOT=Path('build/life-sound')

def read(name):
    x=np.fromfile(ROOT/(name+'.i32'),dtype='<i4').astype(float)
    assert len(x)==32768
    return x-x.mean()

def metrics(x):
    p=np.abs(np.fft.rfft(x*np.hanning(len(x))))**2
    f=np.fft.rfftfreq(len(x),1/44100)
    return dict(rms=float(np.sqrt(np.mean(x*x))),centroid=float((p*f).sum()/p.sum()),high=float(p[f>6000].sum()/p.sum()))

def matched_delta(a,b):
    aa=a/max(np.sqrt(np.mean(a*a)),1);bb=b/max(np.sqrt(np.mean(b*b)),1)
    return float(np.sqrt(np.mean((aa-bb)**2)))

report={}
for eng in range(3):
    base=read(f'engine-{eng}-life-0');bm=metrics(base);series=[]
    for level in [0,25,50,75,100]:
        x=read(f'engine-{eng}-life-{level}');m=metrics(x);m['level']=level;m['rms_ratio']=m['rms']/bm['rms'];m['matched_delta']=matched_delta(base,x)
        assert m['high']<0.01,(eng,level,'new high-frequency layer',m)
        assert .5<m['rms_ratio']<1.25,(eng,level,'level changes excessively',m)
        if level==100:assert m['matched_delta']>.05,(eng,'only volume changed',m)
        series.append(m)
    assert all(series[i]['matched_delta']<series[i+1]['matched_delta'] for i in range(4)),('non-progressive timbral depth',eng)
    report[f'engine-{eng}']=series
    print('life_sound:',eng,'max high band %',round(max(m['high'] for m in series)*100,4),'equal-RMS timbral change',round(series[-1]['matched_delta'],3))

shapes=[read(f'shape-{i}') for i in range(4)];sm=[metrics(x) for x in shapes]
for i in range(1,4):
    assert matched_delta(shapes[0],shapes[i])>.05,('no timbral contrast',i)
    assert sm[i]['centroid']>sm[0]['centroid']*(1.01 if i==1 else 1.1),('not a brighter harmonic shape',i,sm)
    assert sm[i]['high']<.005,('brightness became hiss',i,sm)
    x=shapes[i];p=abs(np.fft.rfft(x*np.hanning(len(x))))**2;f=np.fft.rfftfreq(len(x),1/44100);f0=44100*389/65536
    near=np.abs(f-np.rint(f/f0)*f0)<8
    assert p[near].sum()/p.sum()>.95,('not locked to the receiving note',i)
report['shapes']=sm
(ROOT/'metrics.json').write_text(json.dumps(report,indent=2)+'\n')
# Matched-level examples, with short fades only at file boundaries.
for i,x in enumerate(shapes):
    x=x/np.sqrt(np.mean(x*x))*.18
    fade=np.ones(len(x));fade[:220]=np.linspace(0,1,220);fade[-220:]=np.linspace(1,0,220)
    pcm=np.clip(x*fade*32767,-32767,32767).astype('<i2')
    with wave.open(str(ROOT/f'shape-{i}.wav'),'wb') as w:w.setnchannels(1);w.setsampwidth(2);w.setframerate(44100);w.writeframes(pcm.tobytes())
print('life_sound: equal-level timbre, harmonic locking, progressive range and hiss bounds passed')
