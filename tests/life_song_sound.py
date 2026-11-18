"""Audibility checks on GEN through its UI, not only isolated oscillators."""
from pathlib import Path
import json,wave
import numpy as np
root=Path('build/life-v08-after');dest=Path('build/life-v08-audio');dest.mkdir(exist_ok=True)
report={}
for genre,slug,start,seconds in [('MINIMAL TECHNO','minimal',0,16),('AMBIENT','ambient',3,16),('FUGA','fuga',45,20)]:
    data=[]
    for level in [0,25,50,75,100]:
        x=np.fromfile(root/f'{genre}-{level}.i32',dtype='<i4').reshape(-1,2).astype(float)
        x=x[int(start*44100):int((start+seconds)*44100)];assert len(x)==seconds*44100
        data.append(x)
    target_rms=min(.13,.9/max(np.max(abs(x))/np.sqrt(np.mean(x*x)) for x in data));base=data[0];base_rms=np.sqrt(np.mean(base*base));series=[]
    for level,x in zip([0,25,50,75,100],data):
        rms=np.sqrt(np.mean(x*x));delta=np.sqrt(np.mean((x/rms-base/base_rms)**2))
        mono=x.mean(axis=1);p=abs(np.fft.rfft(mono*np.hanning(len(mono))))**2;f=np.fft.rfftfreq(len(mono),1/44100)
        high=p[f>6000].sum()/p.sum();centroid=(p*f).sum()/p.sum()
        series.append(dict(level=level,rms_ratio=float(rms/base_rms),matched_delta=float(delta),high=float(high),centroid=float(centroid)))
        if level in [0,50,100]:
            # Whole phrase matched, never normalize individual notes or change its dynamics.
            pcm=x/rms*target_rms;peak=np.max(abs(pcm));assert peak<.99,(genre,level,'example would clip',peak)
            fade=np.ones(len(x));fade[:220]=np.linspace(0,1,220);fade[-220:]=np.linspace(1,0,220)
            with wave.open(str(dest/f'{slug}-{level}.wav'),'wb') as w:
                w.setnchannels(2);w.setsampwidth(2);w.setframerate(44100);w.writeframes((pcm*fade[:,None]*32767).astype('<i2').tobytes())
    for m in series:
        assert .5<m['rms_ratio']<1.35,(genre,'level balance',m)
        assert m['high']<=series[0]['high']*1.5+.01,(genre,'new high-frequency layer',m)
    assert series[2]['matched_delta']>.08,(genre,'midrange too subtle',series)
    assert series[4]['matched_delta']>.2,(genre,'top range too subtle',series)
    assert all(series[i]['matched_delta']<series[i+1]['matched_delta'] for i in range(4)),(genre,'not progressive',series)
    report[genre]=series;print(genre,[(m['level'],round(m['matched_delta'],3),round(m['rms_ratio'],3)) for m in series])
(dest/'metrics.json').write_text(json.dumps(report,indent=2)+'\n')
print('life_song: GEN default controls, audible mid/top range, level/hiss bounds PASS')
