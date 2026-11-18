# SPDX-License-Identifier: GPL-3.0-only
"""Audible timbre checks at equal RMS, isolated receiving WAVE (donors not mixed)."""
import json, wave
from pathlib import Path
import numpy as np
root=Path('build/life-extreme')
def read(level,hit=1,classic=0):
    return np.fromfile(root/f'{level}-{hit}-{classic}.i32',dtype='<i4').astype(float)
def rms(x):return float(np.sqrt(np.mean(x*x)))
def difference(a,b):return rms(a/max(rms(a),1)-b/max(rms(b),1))
def high(x):
    p=abs(np.fft.rfft(x*np.hanning(len(x))))**2
    f=np.fft.rfftfreq(len(x),1/44100)
    return float(p[f>6000].sum()/p.sum())
report={'levels':[],'attacks':[]}
dry=read(0)
for level in [0,25,50,75,100]:
    x=read(level)
    assert np.array_equal(x[:64*344],dry[:64*344]),'single voice must not interact with itself'
    d=difference(dry[44100:],x[44100:]);ratio=rms(x)/rms(dry)
    assert high(x)<.005 and .5<ratio<1.25,(level,'hiss/level',ratio)
    report['levels'].append(dict(life=level,equal_rms_change=d,rms_ratio=ratio,high_band=high(x)))
assert report['levels'][-1]['equal_rms_change']>.75,'maximum must transform the sustained receiving timbre'
assert all(a['equal_rms_change']<b['equal_rms_change'] for a,b in zip(report['levels'],report['levels'][1:]))
for classic in [0,1]:
    nohit=read(100,0,classic)
    for hit,name in enumerate(['kick','snare','clap'],1):
        x=read(100,hit,classic);changes=[]
        for start in range(1033*64,len(x)-6000,344*64):
            changes.append(difference(nohit[start:start+4410],x[start:start+4410]))
        assert min(changes)>.15,(classic,name,'attack does not reshape receiving timbre',changes)
        assert high(x)<.005,(classic,name,'hiss')
        report['attacks'].append(dict(classic=bool(classic),hit=name,min_attack_change=min(changes),high_band=high(x)))
# Matched-level, isolated receiving track makes the modulation itself reviewable.
for level in [0,75,100]:
    x=read(level);x=x/max(rms(x),1)*.18
    x[:220]*=np.linspace(0,1,220);x[-220:]*=np.linspace(1,0,220)
    with wave.open(str(root/f'overlap-life-{level}.wav'),'wb') as w:
        w.setnchannels(1);w.setsampwidth(2);w.setframerate(44100);w.writeframes(np.clip(x*32767,-32767,32767).astype('<i2').tobytes())
(root/'metrics.json').write_text(json.dumps(report,indent=2)+'\n')
print('life_extreme: solo bypass, sustained transformation, formula/classic kick/snare/clap attacks and bounded hiss PASS')
