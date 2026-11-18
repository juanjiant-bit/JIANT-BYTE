import subprocess,csv,itertools
from pathlib import Path
root=Path(__file__).resolve().parents[1]
import os
os.chdir(root)
out=root/'docs/byte-v09-cpu.csv'
header='tracks,per_track_limit,life,user_automation,global_budget,audio_ms_per_s,main_ms_per_s,p95_block_us,max_active,max_fading,max_per_track,mean_active,formula_rebuilds_3s,rms'
with out.open('w') as f:
 f.write(header+'\n');f.flush()
 for tracks,limit,life,user in itertools.product([2,4,6],[1,4,8],[0,50,100],[0,1]):
  p=subprocess.run(['build/byte-check/audit_byte_cpu',*map(str,[tracks,limit,life,user,0])],capture_output=True,text=True,check=True)
  f.write(p.stdout);f.flush()
 for life,user in itertools.product([0,100],[0,1]):
  p=subprocess.run(['build/byte-check/audit_byte_cpu','6','8',str(life),str(user),'1'],capture_output=True,text=True,check=True)
  f.write(p.stdout);f.flush()
print('saved',out)
