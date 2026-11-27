/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* The README's screenshots: web/emu/felucca_web.c (the emulator's build of the firmware) natively, driven by a small
 * script on stdin, the screen saved as PPM (tools/gen_byte_readme_images.py builds it, writes the script, makes the
 * PNGs). Commands: ms N (run N ms), shot PATH, sel T, play, stop, keys MASK (bit k: key k, F3..G5), btn MASK (panel.c
 * B_*), enc ROLE N, oct O, nav SECTION PAGE (ui_lens.c nav_put), clay T (DRONE CLAY on, its first sound), chord T V,
 * eng T E P (track T engine E, its preset P),
 * voice T V, gen GENRE SEED ENERGY FORM (web_gen), info, genres, fp T EV (funcplay.c func_apply: a FUNC part's section,
 * mutation, mute, root), fpfill T F (its fill held), fpp T K V (FUNC_PID[K], GROOVE and LAYERS, to V); (visgen) visrec R
 * (HOME > VIS's recipe byte), saver 1 / 0 (MENU > SAVER shown / not), visinfo: built with -DFELUCCA_VIS=1 */
#include "felucca_web.c"
static void run_ms(uint32_t ms){ while(ms--){ web_ms(); ring_r = ring_w; } }
static void shot(const char *path){
    FILE *f = fopen(path, "wb"); uint32_t i;
    fprintf(f, "P6\n240 240\n255\n");
    for(i=0;i<240u*240u;i++){ uint16_t v = web_fb[i]; v = (uint16_t)((v>>8)|(v<<8));
        uint8_t r=(uint8_t)(((v>>11)&31)*255/31), g=(uint8_t)(((v>>5)&63)*255/63), b=(uint8_t)((v&31)*255/31);
        fputc(r,f); fputc(g,f); fputc(b,f); }
    fclose(f); fprintf(stderr, "shot %s\n", path);
}
static int sec_of(const char *s){ for(int i=0;i<SEC_N;i++) if(!strcmp(str_nth(SEC_NAME,(uint32_t)i),s)) return i; return -1; }
int main(void){
    char cmd[64], a[128], line[256];
    web_nor_erase(); web_boot();
    while(fgets(line, sizeof line, stdin)){
        a[0]=0; cmd[0]=0;
        if(sscanf(line, "%63s %127[^\n]", cmd, a) < 1 || cmd[0]=='#') continue;
        if(!strcmp(cmd,"ms")) run_ms((uint32_t)atoi(a));
        else if(!strcmp(cmd,"shot")) shot(a);
        else if(!strcmp(cmd,"sel")) { song.sel=(uint8_t)atoi(a); ui.force=1; }
        else if(!strcmp(cmd,"play")) web_play(1);
        else if(!strcmp(cmd,"stop")) web_play(0);
        else if(!strcmp(cmd,"keys")) web_keys((uint32_t)strtoul(a,0,0));
        else if(!strcmp(cmd,"btn")) web_buttons((uint32_t)strtoul(a,0,0));
        else if(!strcmp(cmd,"enc")) { int r,n; sscanf(a,"%d %d",&r,&n); web_enc(r,n); }
        else if(!strcmp(cmd,"oct")) song.octave=(int8_t)atoi(a);
        else if(!strcmp(cmd,"nav")) { char s[16], nm[64]; sscanf(a,"%15s %63[^\n]",s,nm);
            if(!nav_put(sec_of(s),nm)) fprintf(stderr,"nav fail %s %s\n",s,nm); ui.force=1; }
        else if(!strcmp(cmd,"clay")) { int t=atoi(a); clay_defaults(clay[t]); clay[t][CL_ON]=1; }
        else if(!strcmp(cmd,"clayset")) { int t,f,v; sscanf(a,"%d %d %d",&t,&f,&v); clay[t][f]=(int16_t)v; }
        else if(!strcmp(cmd,"p")) { int t,id,v; sscanf(a,"%d %d %d",&t,&id,&v); trk[t].p[id]=(int16_t)v; }
        else if(!strcmp(cmd,"gen")) { unsigned g,s,e,f; sscanf(a,"%u %u %u %u",&g,&s,&e,&f); web_gen(g,s,e,f); }
        else if(!strcmp(cmd,"info")) { for(int t=0;t<NTRK;t++) fprintf(stderr,"T%d %s voice=%d\n",t+1,ENGINES[trk[t].engine]->name,trk[t].p[P_VOICE]); fprintf(stderr,"page=%s\n", nav_page_name()); }
        else if(!strcmp(cmd,"eng")) { int t,e,pr; sscanf(a,"%d %d %d",&t,&e,&pr); set_engine_of(&trk[t],(uint32_t)e); apply_preset_to(&trk[t],(uint32_t)pr); math_poll(); ui.force=1; }
        else if(!strcmp(cmd,"chord")) { int t,v; sscanf(a,"%d %d",&t,&v); trk[t].p[P_CHRD]=(int16_t)v; }
        else if(!strcmp(cmd,"voice")) { int t,v; sscanf(a,"%d %d",&t,&v); trk[t].p[P_VOICE]=(int16_t)v; }
        else if(!strcmp(cmd,"fp")) { int t,e; sscanf(a,"%d %d",&t,&e); func_apply((uint32_t)t,(uint32_t)e); }
        else if(!strcmp(cmd,"fpfill")) { int t,f; sscanf(a,"%d %d",&t,&f); fp_st[t].fkey=(uint8_t)f; }
        else if(!strcmp(cmd,"fpp")) { int t,k,v; sscanf(a,"%d %d %d",&t,&k,&v); if(k==5) fp_kp[t][0]=(int16_t)v; else trk[t].p[FUNC_PID[k%5]]=(int16_t)v; math_poll(); }
#if FELUCCA_VIS
        else if(!strcmp(cmd,"visrec")) { vis_id[4]=(uint8_t)atoi(a); if(!vis_id[2]) vis_id[2]=VIS_GENRES+1u; ui.force=1; }
        else if(!strcmp(cmd,"saver")) { scrn.st = atoi(a) ? SCR_VIS : SCR_ON; ui.force = 1; }
        else if(!strcmp(cmd,"visinfo")) { vis_recipe_t *r=&vg.r; fprintf(stderr,"set %u sec %u ",vg.ri,vg.sec); fprintf(stderr,"vis content %u skel %u %u %u %u over %u %u %u %u blend %u pal %u def %u/%u %u/%u\n",r->content,r->skel[0],r->skel[1],r->skel[2],r->skel[3],r->over[0],r->over[1],r->over[2],r->over[3],r->blend,r->pal,r->def[0][0],r->amt[0][0],r->def[0][1],r->amt[0][1]); }
#endif
        else if(!strcmp(cmd,"genres")) { for(unsigned g=0;g<NGENRE;g++) fprintf(stderr,"%u %s\n",g+1,GENRE[g].name); }
        else fprintf(stderr,"? %s\n",cmd);
    }
    return 0;
}
