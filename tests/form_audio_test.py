# SPDX-License-Identifier: GPL-3.0-only
"""FORM's actual outputs: level, rhythm, equal-RMS timbral diversity and morph paths."""
import json,wave
from pathlib import Path
import numpy as np
root=Path('build/form')
def read(n):return np.fromfile(root/(n+'.i32'),dtype='<i4').astype(float).reshape(-1,2).mean(1)
def rms(a):return float(np.sqrt(np.mean(a*a)))
def norm(a):return a/max(rms(a),1.)
def env(a):
 n=len(a)//441
 return np.sqrt(np.mean(a[:n*441].reshape(n,441)**2,axis=1))
def spectrum(a):
 n=len(a)//2048
 p=np.mean(abs(np.fft.rfft(a[:n*2048].reshape(n,2048)*np.hanning(2048)))**2,axis=0)
 f=np.fft.rfftfreq(2048,1/44100)
 bands=np.array([p[(f>=lo)&(f<hi)].sum() for lo,hi in [(0,100),(100,300),(300,1000),(1000,3000),(3000,7000),(7000,22051)]])
 return bands/max(bands.sum(),1)
report={'controls':{},'states':{},'morph':{}}
labels=['STATE A','STATE B','MORPH','COLOR','GROOVE','DENSITY','SEED','EVOLVE','DIV','CYCLE','SWING','ROOT','SCALE','CHORD','VOICING','DOMAIN','DRUMS','DETUNE','DELAY']
for k,label in enumerate(labels):
 a,b=[read(f'control-{k:02}-{e}') for e in range(2)]
 change=rms(norm(a)-norm(b));rhythm=rms(norm(env(a))-norm(env(b)))
 spectral=float(np.abs(spectrum(a)-spectrum(b)).sum())
 report['controls'][label]={'equal_rms_delta':change,'envelope_delta':rhythm,'spectral_band_delta':spectral}
 assert change>.06,(label,'ineffective control',change)
 if label in ['GROOVE','DENSITY','DIV','CYCLE','SWING','EVOLVE']:assert rhythm>.03,(label,'timing/structure did not change',rhythm)
for s in range(1,23):
 a=read(f'state-{s:02}');r=rms(a);pk=float(abs(a).max());report['states'][str(s)]={'rms':r/32768,'peak':pk/32768,'bands':spectrum(a).tolist()}
 assert r>300 and pk<32768,(s,r,pk)
 # Listening files retain the same gain as the native render. No per-state loudness normalization.
 with wave.open(str(root/f'state-{s:02}.wav'),'wb') as w:w.setnchannels(1);w.setsampwidth(2);w.setframerate(44100);w.writeframes(np.clip(a,-32767,32767).astype('<i2').tobytes())
for pair in ['byte-float','byte-func']:
 xs=[read(f'morph-{pair}-{j}') for j in range(5)]
 metrics=[]
 for j in range(1,5):
  d=rms(norm(xs[j])-norm(xs[j-1]));assert d>.1,(pair,j,d);metrics.append(d)
 # The intermediate composition must not merely be an output crossfade.
 interp=(xs[0]+xs[4])*.5
 not_only_mix=rms(norm(xs[2])-norm(interp));assert not_only_mix>.1
 report['morph'][pair]={'successive_equal_rms_delta':metrics,'middle_vs_audio_crossfade':not_only_mix}
 for j,x in enumerate(xs):
  with wave.open(str(root/f'morph-{pair}-{j}.wav'),'wb') as w:w.setnchannels(1);w.setsampwidth(2);w.setframerate(44100);w.writeframes(np.clip(x,-32767,32767).astype('<i2').tobytes())
(root/'metrics.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report['controls'],indent=2));print(json.dumps(report['morph'],indent=2));print('FORM: useful levels, every control changes sound, timing controls change rhythm, intermediate composition is not just an audio fade PASS')
