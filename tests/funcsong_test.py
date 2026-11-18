# SPDX-License-Identifier: GPL-3.0-only
"""Composition renders: envelope/rhythm and equal-RMS timbre, not byte differences."""
import json,wave
from pathlib import Path
import numpy as np
root=Path('build/funcsong')
def read(name):return np.fromfile(root/name,dtype='<i4').astype(float).reshape(-1,2).mean(axis=1)
def rms(x):return float(np.sqrt(np.mean(x*x)))
def frame_env(x):
    n=len(x)//441;return np.sqrt(np.mean(x[:n*441].reshape(n,441)**2,axis=1))
def normalized(x):return x/max(rms(x),1)
report={}
for k in range(4):
    a,b=[read(f'p12-k{k}-v{v}.i32')[4410:44100*4] if k==0 else read(f'p12-k{k}-v{v}.i32')[44100*5:] for v in [0,127]]
    change=rms(normalized(a)-normalized(b))
    # Follow the percussion band: bass/chord energy must not hide new hits.
    lo,hi=(700,6000) if k==1 else (6000,18000) if k==2 else (0,22050)
    freq=np.fft.rfftfreq(len(a),1/44100);mask=(freq>=lo)&(freq<hi)
    band=[np.fft.irfft(np.fft.rfft(x)*mask,n=len(x)) for x in (a,b)]
    ea,eb=map(frame_env,band);rhythm=rms(normalized(ea)-normalized(eb))
    assert change>.15,(k,'inaudible waveform/timbre change',change)
    if k in [1,2,3]:assert rhythm>.035,(k,'only timbre, rhythm unchanged',rhythm)
    report['ABCD'[k]]=dict(equal_rms_difference=change,envelope_difference=rhythm,rms0=rms(a),rms127=rms(b),envelope_band_hz=[lo,hi])
for preset in [12,13]:
    x=read(f'p{preset}-k-1-v0.i32');assert rms(x)>100
    pcm=normalized(x)*.18
    pcm[:220]*=np.linspace(0,1,220);pcm[-220:]*=np.linspace(1,0,220)
    with wave.open(str(root/f'func-{ "song" if preset==12 else "beat"}.wav'),'wb') as w:w.setnchannels(1);w.setsampwidth(2);w.setframerate(44100);w.writeframes(np.clip(pcm*32767,-32767,32767).astype('<i2').tobytes())
whole=read('song.i32');env=frame_env(whole)
x=normalized(whole)*.18;x[:220]*=np.linspace(0,1,220);x[-220:]*=np.linspace(1,0,220)
with wave.open(str(root/'func-song-full.wav'),'wb') as w:w.setnchannels(1);w.setsampwidth(2);w.setframerate(44100);w.writeframes(np.clip(x*32767,-32767,32767).astype('<i2').tobytes())
# Sections come from the expression: drums enter after8 beats and leave at48.
report['sections']={str(sec):rms(whole[sec*44100:(sec+2)*44100]) for sec in [0,5,16,25,30]}
assert report['sections']['5']>report['sections']['25']*1.15,'drum section never leaves'
(root/'metrics.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));print('funcsong: A–D alter harmony/rhythm, distinct compositions, and timed section changes PASS')
