/* SPDX-License-Identifier: GPL-3.0-only */
/* MY LOOP: exact source A, new B/C/D, complete playable/saveable arrangement. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static int bad;
static project_t original, part;
/* Compare musical section state and the actual song rows. Retired FM6 bytes can
 * be canonicalized by project import (0 -> default operator scaling) but are
 * not used by any BYTE engine. A section's embedded old chain is not its song. */
static uint32_t arrangement_hash(void){
 uint32_t h=2166136261u;static project_t p;
 for(unsigned i=0;i<4;i++){
  if(!proj_import(&p,&proj_slot[i],sizeof proj_slot[i]))return 0;
  memset(p.fm6,0,sizeof p.fm6);memset(&p.chain,0,sizeof p.chain);p.sum=0;
  h=(h^proj_hash(&p,sizeof p))*16777619u;
 }
 return h^proj_hash(&chain_config,sizeof chain_config)^proj_hash(song_idx.scene[song_cur],sizeof song_idx.scene[0])^proj_hash(song_idx.tsrc[song_cur],sizeof song_idx.tsrc[0])^proj_hash(song_idx.tvar[song_cur],sizeof song_idx.tvar[0]);
}
static void source(void){
 ui_power_on();ui_rec_prefs|=PREF_SIX>>8;rp_apply();
 for(unsigned k=0;k<NTRK;k++){track_defaults_steps(&trk[k]);trk[k].p[P_SLEN]=16;trk[k].p[P_MUTE]=0;}
 host_drums(&trk[0]);dx_kit_load(DX_NKIT);host_preset(&trk[1],ENGI_WAVE,2);
 for(unsigned k=2;k<NTRK;k++){host_preset(&trk[k],ENGI_LOFI,1);track_defaults_steps(&trk[k]);}
 for(unsigned i=0;i<16;i++){
  step_t*s=&trk[0].step[i];memset(s,0,sizeof *s);s->time=ST_NOTE;s->hit=i%4==0?1:i%4==2?8:0;s->vel=75+i;
  step_t*b=&trk[1].step[i];memset(b,0,sizeof *b);b->time=i%4?ST_REST:ST_NOTE;b->n=i%4?0:1;b->note[0]=36+i/4*2;b->vel=90+i;
 }
 trk[1].p[P_SSWING]=13;trk[1].p[P_SGATE]=73;voice_limit[1]=3;song.g[G_BPM]=129;song.g[G_SWING]=11;
 str_cpy(dx_text[0],"sin(w*1.5)",DX_TEXT);dx_form[0]=DX_USER;dx_fid[0]=0;dx_fmod[0][DFM_BEND]=27;
 motion_set_event(&trk[1],0,P_FCUT,80);motion_set_event(&trk[1],12,P_FCUT,110);
 life_v[LIFE_AMT]=0;project_capture(&original);
 gp.form=3;gp.genre=1;gp.seed=321;gp.energy=65;gp.complex=60;gp.human=GA_AUTO;gp.swing=GA_AUTO;
 for(unsigned k=0;k<GP_N;k++)gp.amt[k]=GA_AUTO;
}
static int same_loop(const project_t*p){return !memcmp(original.t,p->t,sizeof original.t)&&!memcmp(original.dx,p->dx,sizeof original.dx)&&!memcmp(original.dx_text,p->dx_text,sizeof original.dx_text)&&!memcmp(original.dx_fmod,p->dx_fmod,sizeof original.dx_fmod)&&!memcmp(&original.motion,&p->motion,sizeof original.motion)&&original.g[G_BPM]==p->g[G_BPM]&&original.g[G_SWING]==p->g[G_SWING]&&!memcmp(original.voice_limit,p->voice_limit,NTRK);}
int main(void){
 source();gen_go();printf("MY LOOP state: mode=%u sections=%u rows=%u message=%s / %s\n",live.mode,dice_secs,chain_config.count,ui.msg,ui.msg2);
 bad+=check("MY LOOP creates SONG with four real sections and multiple arrangement rows",live.mode&&dice_secs==15&&chain_config.count>=6);
 int ok=proj_import(&part,&proj_slot[0],sizeof proj_slot[0]);bad+=check("source A preserves all notes, empty tracks, gates, velocity, sounds, formula text, limits and automation",ok&&same_loop(&part));
 project_capture(&part);bad+=check("current loop remains the exact original after creating the song",same_loop(&part));
 unsigned differing=0;for(unsigned s=1;s<4;s++){ok=proj_import(&part,&proj_slot[s],sizeof proj_slot[s]);differing+=ok&&memcmp(original.t,part.t,sizeof original.t)!=0;}
 bad+=check("new structure lives in distinct B/C/D sections",differing==3);
 uint32_t h=arrangement_hash();source();gen_go();bad+=check("same source and seed reproduce the same arrangement",h==arrangement_hash());
 gen_go();bad+=check("repeat GEN from retained source A keeps the same musical arrangement",h==arrangement_hash());
 nav_put(SEC_HOME,"GEN 1");frame();press(B_PLAY);events_block(CTL);song_poll();
 unsigned seen=0,transitions=0,last=99,expected=0;for(unsigned r=0;r<chain_config.count;r++)expected|=1u<<chain_config.row[r].slot;
 for(unsigned b=0;b<600;b++){
  if(chain.row!=last){last=chain.row;transitions++;seen|=1u<<live.cur;
   if(live.cur==0){ok=dx_form[0]==DX_USER&&!strcmp(dx_text[0],"sin(w*1.5)")&&dx_fmod[0][DFM_BEND]==27&&voice_limit[1]==3;bad+=check("arrangement restores source formula/modifiers/voice limit on A",ok);}
   if(transitions>1&&chain.row==0)break;
  }
  events_block(chain_bar());song_poll();
 }
 printf("playback: running=%u transitions=%u rows=%u seen=%u row=%u\n",chain.running,transitions,chain_config.count,seen,chain.row);
 bad+=check("PLAY traverses the complete arrangement and returns to its first row",chain.running&&transitions==chain_config.count+1u&&seen==expected&&(seen&1u)&&__builtin_popcount(seen)>=3);
 seq_stop();song_poll();
 for(unsigned s=0;s<4;s++){static project_store_t out;ok=proj_import(&part,&proj_slot[s],sizeof proj_slot[s])&&proj_pack(&out,&part)&&proj_import(&part,&out,sizeof out);bad+=check("each generated section survives SAVE/LOAD encoding",ok);}
 return !!bad;
}
