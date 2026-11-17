/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* Musical LIFE: post-synthesis processing, exact bypass, bounded dynamics, clock lock,
 * independent controls, normalized LINK and identical oscillator/formula state. */
#define main hostsim_main
#include "hostsim.c"
#undef main
static int fails;
#define CHECK(c,s) do { printf("lifeb: %-76s %s\n",s,(c)?"ok":"FAIL");if(!(c))fails++; } while(0)
#define NB 4096u
#define NS (NB * CTL)
static int32_t dry[NS], lwet[NS];
static uint32_t os0[16], os1[16];
static void setup(int amt,int pulse,int color,int link) {
    memset(trk,0,sizeof trk); host_tracks_init(); life_reset();
    memset(track_env,0,sizeof track_env); memset(&MOD,0,sizeof MOD);
    life_defaults(); life_v[LIFE_AMT]=amt;life_v[LIFE_PULSE]=pulse;life_v[LIFE_COLOR]=color;life_v[LIFE_LINK]=link;
    song.g[G_BPM]=120;song.g[G_CLOCK]=0;song.playing=1;clk_n=clk_pos=0;mns_clk=0;track_follow_mask=0;
    for(unsigned k=0;k<NTRK;k++){trk[k].p[P_MUTE]=0;life_v[LIFE_N+k]=100;}
}
static void clock_at(uint32_t sample) {
    uint32_t step=div_samples(2);clk_n=sample/step;clk_pos=sample%step;mns_clk++;
}
static double rms(const int32_t*a) {double e=0;for(unsigned i=0;i<NS;i++)e+=(double)a[i]*a[i];return sqrt(e/NS);}
static double correlation(const int32_t*a,const int32_t*b) {double ab=0,aa=0,bb=0;for(unsigned i=0;i<NS;i++){ab+=(double)a[i]*b[i];aa+=(double)a[i]*a[i];bb+=(double)b[i]*b[i];}return ab/sqrt(aa*bb);}
/* Synthetic bandlimited source through precisely the stage used by every engine. */
static void stage(int amt,int p,int c,int l,int per,int32_t*out) {
    int32_t b[CTL];setup(amt,p,c,l);life_v[LIFE_N]=per;dx_xon=0;
    for(unsigned k=0;k<NB;k++){
        clock_at(k*CTL);track_env[1]=16000;life_master();
        for(unsigned i=0;i<CTL;i++){unsigned n=k*CTL+i;b[i]=sine_i(n*42852281u)/2+sine_i(n*214261405u)/4;}
        life_process(0,b,CTL,life_pt(0,0));memcpy(out+k*CTL,b,sizeof b);
    }
}
/* Engine state and samples, with identical notes/presets. The global FX are deliberately absent. */
static void engine(unsigned e,int amt,int32_t*out,uint32_t*state) {
    int32_t b[CTL];setup(amt,100,100,100);rng_state=1234;
    host_preset(&trk[0],e,e==ENGI_WAVE?1:9);
    trk[0].p[P_DIST]=trk[0].p[P_CHOR]=trk[0].p[P_DLY]=trk[0].p[P_REV]=0;
    trk[0].p[P_DRIFT]=127;trk_note_on(&trk[0],60,100);
    for(unsigned k=0;k<NB;k++){
        clock_at(k*CTL);track_env[1]=16000;life_master();track_render(&trk[0],b,CTL);memcpy(out+k*CTL,b,sizeof b);
    }
    memset(state,0,16*sizeof *state);memcpy(state,trk[0].v[0].s,sizeof trk[0].v[0].s < 16*sizeof *state ? sizeof trk[0].v[0].s : 16*sizeof *state);
}
int main(void) {
    int monotonic=1;for(int i=1;i<=100;i++)if(life_curve(i)<life_curve(i-1))monotonic=0;
    CHECK(monotonic && life_curve(0)==0 && life_curve(100)==256 && life_curve(50)==48 && life_curve(75)==126,
          "audio-pot taper is monotonic, exact at endpoints and broad in the middle");
    setup(0,0,0,0);CHECK(life_def(LIFE_AMT)==0,"power-on LIFE default is zero");
    stage(0,100,100,100,100,dry);stage(100,0,0,0,100,lwet);
    CHECK(!memcmp(dry,lwet,sizeof dry),"zero controls are bit-identical to LIFE off");
    stage(100,100,100,100,0,lwet);CHECK(!memcmp(dry,lwet,sizeof dry),"per-track LIFE zero is an exact bypass");
    for(unsigned k=0;k<4;k++){
        stage(100,k==0||k==3?100:0,k==1||k==3?100:0,k==2||k==3?100:0,100,lwet);
        double r=rms(lwet)/rms(dry),co=correlation(dry,lwet);int peak=0;
        for(unsigned i=0;i<NS;i++)if(abs(lwet[i])>peak)peak=abs(lwet[i]);
        printf("lifeb: control %u RMS %.4fx, correlation %.6f, peak %d\n",k,r,co,peak);
        CHECK(memcmp(dry,lwet,sizeof dry)&&r>0.2&&r<0.95&&co>0.65&&peak<=24576,"each control makes a substantial bounded change without losing the source");
    }
    stage(100,65,60,40,100,lwet);
    double d=0;for(unsigned i=0;i<NS;i++){double x=(double)lwet[i]-dry[i];d+=x*x;}
    CHECK(sqrt(d/NS)/rms(dry)>0.12,"turning up master LIFE with default controls gives an audible difference");
    setup(100,100,0,0);
    for(unsigned bpm=60;bpm<=180;bpm+=60){song.g[G_BPM]=bpm;unsigned step=div_samples(2);
        clock_at(4*step);life_master();unsigned a=life.pulse;clock_at(6*step);life_master();unsigned b=life.pulse;
        CHECK(a<=1&&b>=1022,"PULSE locks to the beat at 60/120/180 BPM, full gain on beats");
    }
    setup(100,0,0,100);trk_live=2;track_env[1]=16000;
    for(unsigned i=0;i<100;i++)life_master();unsigned two=life.link[0];
    trk_live=NTRK;for(unsigned k=1;k<NTRK;k++)track_env[k]=16000;
    for(unsigned i=0;i<100;i++)life_master();unsigned many=life.link[0];
    CHECK(abs((int)two-(int)many)<=4,"LINK averages source envelopes: more tracks do not multiply depth");
    for(unsigned e=0;e<2;e++){engine(e?ENGI_LOFI:ENGI_WAVE,0,dry,os0);engine(e?ENGI_LOFI:ENGI_WAVE,100,lwet,os1);
        CHECK(!memcmp(os0,os1,sizeof os0),"LIFE leaves oscillator/formula phase and state unchanged");
        CHECK(memcmp(dry,lwet,sizeof dry)&&rms(lwet)>0.2*rms(dry),"WAVE/MATH are modulated without losing the voice");
    }
    /* Stereo image sides must receive the same transfer as the mono sum. */
    setup(100,100,100,100);dx_xon=1;int32_t b[CTL];int stereo=1;
    for(unsigned k=0;k<40;k++){clock_at(k*CTL);life_master();for(unsigned i=0;i<CTL;i++)b[i]=dx_sl[i]=dx_sr[i]=sine_i((k*CTL+i)*40000000u);
        life_process(0,b,CTL,256);stereo&=!memcmp(b,dx_sl,sizeof b)&&!memcmp(b,dx_sr,sizeof b);}
    CHECK(stereo,"stereo image stays coherent: sum and sides use the same processing");
    puts(fails?"lifeb: FAILED":"lifeb: all ok");return !!fails;
}
