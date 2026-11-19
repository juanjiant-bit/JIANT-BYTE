/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (0.8.5) EDIT's lens panels (ui_lens.c), as the approved mock-up's; included by ui_graph.c. The canvas is the
 * lens's panel (240 x 140, x 4..236).
 *   SOUND   (EDIT 1 / EDIT 2 of a synth) the engine's being on the left (graph_being's, alive with the sound), the
 *           engine's page name over it and the sound's name under it; on the right the scope in a well and the eight
 *           E parameters as chips, the page's four lit. DRUM's KIT keeps its colony (graph_colony)
 *   FILTER  the response of TYPE at CUT with RES (LP HP BP COMB), CUT a line, DRIVE a row of cells
 *   VOICE   the track's voices one a row, each as full as its envelope now; VOICE and PRIO (VOICE 2: ALLOC); GLIDE's
 *           curve
 *   FOCUS   the parameter as a gauge, its value large, the MOD slot that moves it ("M1 -> +63%"); E1..E8 as tabs
 * The others (MORPH, RETRIG, ROTOR, the kit's, the operators') keep their panels. */
#define ED_CH 17                                        /* a chip's height (two lines of AF_X) */
/* a chip: the stripe, the label, the value (on: the page's) */
static void ed_chip(int32_t x, int32_t y, int32_t w, const param_desc_t *d, int16_t v, int on)
{
    char b[24];
    const char *u = "";
    uint16_t bg = on ? T_TINT : T_RAISE;
    cv_rect(x, y, w, ED_CH - 1, bg);
    cv_rect(x, y, 2, ED_CH - 1, on ? T_THEME : T_LINE);
    if (!d || !d->label || d->label[0] == '-')
        return;
    param_format(d, v, b, &u);
    str_cpy(b + str_len(b), u, 24 - str_len(b));
    cv_clip_text(x + 5, y - AF_X_CAP_Y + 1, &AF_X, d->label, T_TEXT, bg, w - 6);
    cv_clip_text(x + 5, y - AF_X_CAP_Y + 9, &AF_X, b, on ? T_TEXT : T_DIM, bg, w - 6);
}
static void ed_cap(int32_t x, int32_t y, const char *s, uint16_t c)   /* a caption, its capitals from y */
{
    cv_text(x, y - AF_X_CAP_Y, &AF_X, s, c);
}
static __attribute__((noinline)) void graph_sound_lz(const track_t *t)
{
    int32_t x = 122, w = 106, ty = 2;
    /* (V10.7.15, the owner) no parameter list beside the being: the knobs' cards above say the same. SCOPE OFF (the
     * default): the creature (or the generic being) on the whole panel; SCOPE ON: the being left, the scope tall right */
    if (!ui_scope || trk_engp(t) == &ENG_LOFI || trk_engp(t) == &ENG_WAVE) {   /* (JIANT BYTE: a formula's panel whole) */
        if (!vx_draw(t, 4, 2, 232, (int32_t)cv_h - 4, -1))
            graph_being_at(t, 120 * 16, ((int32_t)cv_h / 2) * 16, 34 * 16);
    } else {
        if (!vx_draw(t, 8, 14, 108, 112, -1))          /* (0.9.4 / 0.9.6: the engines' own creatures: ui_engvis.c) */
            graph_being_at(t, 60 * 16, 68 * 16, 22 * 16);
        cv_rect(0, 0, 4, (int32_t)cv_h, T_BG);
        cv_rect(116, 0, 60, (int32_t)cv_h, T_SURF);
        /* (V10.7.19, the owner: the users like the trace on the background) no tile behind the scope in any skin
         * (COLORS drew a RAISE card): the trace and its title on the panel itself */
        GFX_HOOK_ALIGN(0, ty + 4, 0, ty + 4 + AF_X_CAP_H, AL_V | AL_PASS, "EDIT scope title");
        cv_text_on(x, ty + 4 - AF_X_CAP_Y, &AF_X, "SCOPE", T_TEXT, T_SURF);
        scope_at(x + 4, w - 8, ty + 68, 48, T_THEME);
    }
    /* (the page's name stood at the top left and the sound's at the foot: the breadcrumb and the header say them) */
}
/* FILTER: y of the response at x (px, 10..226), the cut at xc, res 0..1000; the unity line at y 62 */
static int32_t ed_resp(uint32_t ty, int32_t x, int32_t xc, int32_t res)
{
    int32_t d = x - xc, a = d < 0 ? -d : d, pk = res * 46 / 1000, y;
    int32_t bump = pk * 64 / (64 + d * d / 2);
    if (ty == 3u || ty == 4u)                           /* COMB: teeth CUT apart, as deep as RES (COMB-: between) */
        return 62 - (int32_t)(((ty == 4u ? -1 : 1) * og_cos((uint32_t)x * (uint32_t)(4096 + xc * 160)) * (8 + pk / 2)) >> 15);
    if (ty == 5u || ty == 6u)                           /* (V10.22) LP24 (filters2: MS20's 5 its alias): twice as steep */
        return clamp(62 + (d > 0 ? 4 * d : 0) - bump, 8, 104);
    y = ty == 0u ? (d > 0 ? 2 * d : 0) : ty == 1u ? (d < 0 ? 2 * a : 0) : 2 * a / 3;   /* LP HP BP */
    return clamp(62 + y - bump - (ty == 2u ? pk / 3 : 0), 8, 104);
}
static void graph_filter_lz(const track_t *t, const page_t *pg)
{
    static const char *const FX[3] = {"100", "1k", "10k"};
    int16_t *vp;
    const param_desc_t *d;
    int32_t r[4], x, py = 0, xc, k, nd;
    uint32_t ty = ft_of(t->p);
    char b[32];
    for (k = 0; k < 4; k++) {
        d = page_desc(pg, (uint32_t)k, &vp);
        r[k] = d ? clamp(RATIO(d, enum_rank(d, *vp)), 0, 1000) : 0;
    }
    xc = 10 + r[1] * 216 / 1000;
    for (k = 0; k < 3; k++)
        cv_rect(64 + k * 54, 6, 1, 99, T_RAISE);
    for (x = 10; x <= 226; x++) {
        int32_t y = ed_resp(ty, x, xc, r[2]);
        cv_rect(x, y, 1, 105 - y, ux_mix(T_SURF, heat_col(40 + x * 160 / 226), 30));
        if (x > 10)
            cv_line_t(x - 1, py, x, y, T_THEME, 2);
        py = y;
    }
    for (k = 0; k < 3; k++)
        cv_text(67 + k * 54, 108 - AF_X_CAP_Y, &AF_X, FX[k], T_TEXT);
    cv_rect(xc, 6, 1, 99, T_ACCENT);
    og_node(xc * 16, ed_resp(ty, xc, xc, r[2]) * 16, 4, T_ACCENT);
    str_cpy(b, N_FTYPE[ty], 32);
    str_cpy(b + str_len(b), trk_eng(t) == 0u ? " \xB7 PER VOICE" : " \xB7 ON THE PART", 32 - str_len(b));
    ed_cap(10, 6, b, T_TEXT);
    d = page_desc(pg, 3, &vp);
    if (d) {
        const char *u = "";
        str_cpy(b, d->label, 32);
        str_cpy(b + str_len(b), " ", 2);
        param_format(d, *vp, b + str_len(b), &u);
        str_cpy(b + str_len(b), u, 32 - str_len(b));
        ed_cap(10, 120, b, T_TEXT);
    }
    nd = r[3] * 20 / 1000;
    for (k = 0; k < 20; k++)
        cv_rect(110 + k * 6, 116, 4, 12, k < nd ? T_THEME : T_RAISE);
}
static void graph_voice_lz(const track_t *t, const page_t *pg)
{
    uint32_t i, nv = trk_nvoice(t) ? trk_nvoice(t) : NVOICE;
    int32_t glide = t->p[P_GLIDE], py = 0, x;
    const char *u = "";
    char b[40];
    uint32_t active = 0;
    for (i = 0; i < NVOICE; i++) active += t->v[i].active && t->v[i].stage != 4u;
    fmt_int(b, (int32_t)active); str_cpy(b + str_len(b), " ACTIVE / ", 40 - str_len(b));
    fmt_int(b + str_len(b), (int32_t)nv); str_cpy(b + str_len(b), " MAX", 40 - str_len(b));
    ed_cap(10, 6, b, T_TEXT);
    for (i = 0; i < NVOICE; i++) {
        int32_t y = 17 + (int32_t)i * 9, lv = 0;
        char n[2] = {(char)('1' + i), 0};
        if (t->v[i].stage && t->v[i].stage < 4u)
            lv = (int32_t)(((int64_t)t->v[i].env * 196) >> 24);
        ed_cap(14, y, n, i < nv ? T_TEXT : T_DIM);
        cv_rect(26, y, 202, 7, i < nv ? T_RAISE : T_LINE);
        if (lv > 0)
            cv_rect(26, y, lv, 7, heat_col(60 + lv));
    }
    param_format(track_desc(t, P_VOICE), t->p[P_VOICE], b, &u);
    str_cpy(b + str_len(b), pg->id[0] == P_ALLOC ? " \xB7 ALLOC " : " \xB7 PRIO ", 40 - str_len(b));
    param_format(track_desc(t, pg->id[0] == P_ALLOC ? P_ALLOC : P_PRIO), t->p[pg->id[0] == P_ALLOC ? P_ALLOC : P_PRIO],
                 b + str_len(b), &u);
    ed_cap(10, 95, b, T_TEXT);
    str_cpy(b, "GLIDE ", 40);
    param_format(track_desc(t, P_GLIDE), t->p[P_GLIDE], b + 6, &u);
    str_cpy(b + str_len(b), u, 40 - str_len(b));
    ed_cap(10, 120, b, T_TEXT);
    for (x = 0; x <= 104; x++) {                        /* the glide: low note to high, as slow as GLIDE */
        int32_t tm = glide ? x * 127 * 6 / (glide * 4 + 1) : 1000, k = tm > 255 ? 255 : tm;
        int32_t y = x < 16 ? 130 : 130 - 22 * (k * (255 - (255 - k) * (255 - k) / 255)) / 65025;
        if (x)
            cv_line_t(122 + x - 1, py, 122 + x, y, T_THEME, 2);
        py = y;
    }
}
/* (V10.7.33, the owner) EDIT > AUTO KNOBS (was FOCUS): "pick a knob and what moves it". On top the knob picked (KNOB 1) and its
 * value (KNOB 2) as a bar of the thermal ramp (heat_col); under it what moves it and how much (KNOB 3 SRC, KNOB 4 AMT: the
 * MOD slot aimed at it, the same MATRIX shows); the eight knobs at the foot, the one picked lit. (The gauge's circle gone) */
static void focus_bar(int32_t y, int32_t f)             /* a track 220 px, 0..f (0..1000) of it in the ramp (heat_bar) */
{
    cv_rect(10, y, 220, 12, T_RAISE);
    heat_bar(10, y, f * 220 / 1000, 220, 12, T_THEME);
}
static void graph_focus_lz(const track_t *t)
{
    uint32_t k = focus_k(), i;
    int32_t j = focus_slot(k), a = 0;
    const param_desc_t *d = track_desc(t, P_E0 + k);
    const char *u = "";
    char b[24];
    param_format(d, t->p[P_E0 + k], b, &u);
    str_cpy(b + str_len(b), u, 24 - str_len(b));
    cv_text(10, 6, &AF_M, d->label, T_TEXT);
    cv_text_r(230, 6, &AF_M, b, T_THEME, T_SURF);
    focus_bar(28, clamp(RATIO(d, enum_rank(d, t->p[P_E0 + k])), 0, 1000));
    ed_cap(10, 54, "MOVED BY", T_TEXT);
    if (j >= 0) {
        const param_desc_t *da = track_desc(t, P_M1AMT + 3u * (uint32_t)j);
        a = t->p[P_M1AMT + 3u * (uint32_t)j];
        str_cpy(b, N_MSRC[clamp(t->p[P_M1SRC + 3u * (uint32_t)j], 0, MS_N - 1)], 24);
        cv_text_r(230, 64, &AF_M, b, T_THEME, T_SURF);
        param_format(da, a, b, &u);
        str_cpy(b + str_len(b), u, 24 - str_len(b));
        cv_text(10, 64, &AF_M, b, T_TEXT);
        a = RATIO(da, a < 0 ? -a : a);
        a = a * 2 - 1000;                               /* (as much as it moves: a bipolar AMT's size) */
    } else
        cv_text(10, 64, &AF_M, "NOTHING", T_DIM);
    focus_bar(86, a);
    for (i = 0; i < 8u; i++) {                          /* E1..E8 */
        const param_desc_t *di = track_desc(t, P_E0 + i);
        int32_t x = 8 + (int32_t)i * 28;
        uint16_t bg = i == k ? T_THEME : T_RAISE;
        cv_rect(x, 118, 26, 13, bg);
        if (di && di->label && di->label[0] != '-')
            cv_clip_text(x + 2, 121 - AF_X_CAP_Y, &AF_X, di->label, i == k ? T_INK : T_TEXT, bg, 23);
    }
}
/* FORM visual language: equations, event masks, arrangement and harmony, not a repeated preset list. */
static void form_cap(int32_t x,int32_t y,const char*s,uint16_t c){cv_text_on(x,y-AF_S_CAP_Y,&AF_S,s,c,T_BG);}
static void form_num(int32_t x,int32_t y,int32_t n,const char*s,uint16_t c){char b[24];fmt_int(b,n);str_cpy(b+str_len(b),s,sizeof b-str_len(b));form_cap(x,y,b,c);}
static uint32_t form_view_clock(const track_t*t){
 if(func_song(t))return func_clock_begin;
 for(uint32_t i=0;i<NVOICE;i++)if(t->v[i].active)return (uint32_t)(((uint64_t)t->v[i].ph[0]<<16)/((uint32_t)FS*60u/(uint32_t)clamp(song.g[G_BPM],20,300)));
 return 0;
}
static void form_wave_trace(uint32_t state,int32_t color,int32_t x,int32_t y,int32_t w,int32_t h,uint16_t ink){
 int32_t py=y;cv_rect(x,y+h/2,w,1,T_LINE);
 for(int32_t i=0;i<w;i+=2){int32_t v=form_wave(state,(uint32_t)i,0x100000000ull/(uint32_t)w,0,color,0);int32_t yy=y+h/2-v*(h/2-1)/32768;if(i)cv_line_t(x+i-2,py,x+i,yy,ink,1);py=yy;}
}
static void form_state_card(const track_t*t,uint32_t state,int32_t y,uint16_t ink){
 uint32_t mode=(uint32_t)clamp(t->p[P_CAOS],0,10),dom=mode?mode:FORM_STATE[state].domain;
 cv_rect(6,y,3,32,ink);form_cap(14,y+1,FORM_NAMES[state],T_TEXT);form_cap(14,y+17,FORM_DOMAIN[dom],ink);
 form_wave_trace(state,clamp(t->p[P_E3]+FORM_STATE[state].color-64,0,127),164,y,68,31,ink);
}
static __attribute__((noinline)) void form_states_panel(const track_t*t,uint32_t a,uint32_t b,uint32_t m){
 form_state_card(t,a,3,T_ACCENT);form_state_card(t,b,42,T_THEME);
 form_cap(8,84,"A",T_ACCENT);form_cap(224,84,"B",T_THEME);
 for(int32_t i=0;i<26;i++)cv_rect(26+i*7,84,5,10,ux_mix(T_ACCENT,T_THEME,i*4));
 int32_t pos=26+(int32_t)m*180/127;cv_rect(pos,80,3,18,T_TEXT);
 form_num(8,107,(127-(int32_t)m)*100/127,"% A",T_ACCENT);form_num(179,107,(int32_t)m*100/127,"% B",T_THEME);
 form_cap(8,127,t->p[P_LRTRG]?"PLAY: COMPOSITION":"HOLD KEY: COMPOSITION",T_DIM);
}
static __attribute__((noinline)) void form_rules_panel(const track_t*t,uint32_t a,uint32_t b,uint32_t m,uint32_t clock){
 static const char*const lanes[]={"KICK","SNARE","HAT"};
 uint32_t cyc=clock/((uint32_t)clamp(t->p[P_SLEN],1,64)<<16),seed=(uint32_t)t->p[P_E6],groove=(uint32_t)t->p[P_E4];
 form_cap(8,4,"PERCUSSION / ONE BAR",T_TEXT);
 for(uint32_t lane=0;lane<3;lane++){
  uint32_t ma=lane==0?FORM_STATE[a].kick:lane==1?FORM_STATE[a].snare:FORM_STATE[a].hat;
  uint32_t mb=lane==0?FORM_STATE[b].kick:lane==1?FORM_STATE[b].snare:FORM_STATE[b].hat;
  ma=form_pattern(ma,groove,seed,cyc);mb=form_pattern(mb,groove,seed,cyc);form_cap(8,27+(int32_t)lane*20,lanes[lane],T_DIM);
  for(uint32_t j=0;j<16;j++){int32_t amp=0,age=form_event(ma,mb,j,0,m,(uint32_t)t->p[P_E5],seed+lane*19u,&amp);if(age)amp=0;cv_rect(64+(int32_t)j*10,24+(int32_t)lane*20,8,12,amp?ux_mix(T_RAISE,T_THEME,20+amp*80/32767):T_RAISE);}
 }
 form_cap(8,90,"SECTION ENTRIES",T_TEXT);
 for(uint32_t j=0;j<8;j++){int32_t q=form_mix(form_section(a,j<<13,2,(uint32_t)t->p[P_E7]),form_section(b,j<<13,2,(uint32_t)t->p[P_E7]),m);cv_rect(8+(int32_t)j*28,105,24,9,ux_mix(T_RAISE,T_ACCENT,q*100/32767));}
 form_cap(8,127,"SEED SHIFTS / EVOLVE ARRANGES",T_DIM);
}
static __attribute__((noinline)) void form_clock_panel(const track_t*t,uint32_t a,uint32_t b,uint32_t m,uint32_t clock){
 static const char*const layers[]={"MELODY","HARMONY","DRUMS"};uint32_t cycle=(uint32_t)clamp(t->p[P_SLEN],1,64),part=(clock%(cycle<<16))*8u/(cycle<<16);
 form_cap(8,4,t->p[P_LRTRG]?"SONG / PLAY TRANSPORT":"NOTE / KEY TRANSPORT",T_TEXT);
 form_num(8,22,(int32_t)cycle," BEATS",T_ACCENT);char div[24]="DIV ";const char*unit="";param_format(track_desc(t,P_SDIV),t->p[P_SDIV],div+4,&unit);str_cpy(div+str_len(div),unit,sizeof div-str_len(div));form_cap(118,22,div,T_DIM);
 for(uint32_t layer=0;layer<3;layer++){form_cap(8,43+(int32_t)layer*17,layers[layer],T_DIM);for(uint32_t j=0;j<8;j++){int32_t q=form_mix(form_section(a,j<<13,layer,(uint32_t)t->p[P_E7]),form_section(b,j<<13,layer,(uint32_t)t->p[P_E7]),m);cv_rect(72+(int32_t)j*20,40+(int32_t)layer*17,17,10,ux_mix(T_RAISE,layer==2?T_ACCENT:T_THEME,q*100/32767));} }
 cv_rect(72+(int32_t)part*20,36,17,2,T_TEXT);
 form_cap(8,99,"SWING",T_TEXT);cv_rect(63,104,166,1,T_LINE);for(uint32_t j=0;j<8;j++){int32_t x=64+(int32_t)j*20+(j&1u?t->p[P_SSWING]*9/100:0);cv_rect(x,98,3,j&1u?8:14,j&1u?T_THEME:T_ACCENT);}
 form_cap(8,127,"DIV: SPEED / CYCLE: FORM LENGTH",T_DIM);
}
static __attribute__((noinline)) void form_notes_panel(const track_t*t,uint32_t a,uint32_t b,uint32_t m,uint32_t clock){
 /* (FORM 2) the notes as they sound: eng_form.c form_step (the harmony, the melody's gene, the voicing), the nearer
  * state's; HARMONY's mode at the top, the chord of this section with its VOICING */
 uint32_t cycle=(uint32_t)clamp(t->p[P_SLEN],1,64),cyc=clock/(cycle<<16),section=(clock%(cycle<<16))/cycle>>13,seed=(uint32_t)clamp(t->p[P_E6],0,127),ev=(uint32_t)clamp(t->p[P_E7],0,127);
 uint32_t mutation=((fvm_hash(seed+cyc*37u+section*7u)>>(28u-ev/43u))&7u)*ev/127u,id=m<64u?a:b,hm=t->p[P_AORDER]?(uint32_t)clamp(t->p[P_AORDER],0,5):0u;
 int32_t root=48+t->p[P_ROOT],prev=0;form_step_t fs;
 form_cap(8,4,"MELODY",T_TEXT);form_cap(90,4,"HARMONY",T_DIM);form_cap(160,4,hm?FORM_HARM[hm]:FORM_HARM[1u+(t->p[P_AGATE]>0&&FORM_GENE[id].harm==FH_CLASSIC?FH_FAMILY:FORM_GENE[id].harm)],T_ACCENT);
 cv_rect(8,61,224,1,T_LINE);
 for(uint32_t i=0;i<16;i++){form_step(t,id,id,i,section,cyc,seed,mutation,root,&fs);if(fs.rest){prev=0;continue;}
  int32_t y=61-clamp(fs.mn-root,0,36);if(i&&prev)cv_line_t(10+(int32_t)(i-1)*14,prev,10+(int32_t)i*14,y,T_DIM,1);cv_rect(8+(int32_t)i*14,y-2,6,5,T_THEME);prev=y;}
 form_cap(8,62,"PROGRESS",T_DIM);form_cap(90,62,FORM_PROG[clamp(t->p[P_AGATE],0,8)],T_TEXT);form_cap(8,75,"CHORD",T_TEXT);form_cap(90,75,"VOICING",T_DIM);form_cap(160,75,FORM_VOICE[clamp(t->p[P_CVOI],0,7)],T_ACCENT);
 form_step(t,id,id,0,section,cyc,seed,mutation,root,&fs);
 uint32_t count=FORM_STATE[id].voices>1?(t->p[P_CHRD]?(uint32_t)clamp(FORM_STATE[id].voices,1,4):1u):0u;
 if(!count)form_cap(8,99,"NO CHORD LAYER IN THIS STATE",T_DIM);
 for(uint32_t k=0;k<count;k++){char name[8];note_name(name,(uint32_t)fs.cn[k]);cv_rect(8+(int32_t)k*56,94,50,18,T_RAISE);form_cap(16+(int32_t)k*56,99,name,T_TEXT);}
 form_cap(8,127,"MELODY + HARMONY FOLLOW SCALE",T_DIM);(void)b;
}
static __attribute__((noinline)) void form_layers_panel(const track_t*t,uint32_t a,uint32_t b,uint32_t m,uint32_t layers){
 /* (FORM 2) TIMBRE: the styles of A and B, DRIVE TONE WOBBLE; LAYERS: the VOICING, the drums, DETUNE, the LEVEL */
 uint32_t mode=(uint32_t)clamp(t->p[P_CAOS],0,10),da=mode?mode:FORM_STATE[a].domain,db=mode?mode:FORM_STATE[b].domain;
 uint32_t hasdrums=(m<127&&(FORM_STATE[a].kick|FORM_STATE[a].snare|FORM_STATE[a].hat))||(m>0&&(FORM_STATE[b].kick|FORM_STATE[b].snare|FORM_STATE[b].hat));
 const char*labs[3];int32_t val[3];
 if(layers){
  form_cap(8,4,"LAYERS",T_TEXT);form_cap(8,24,"OCTAVE",T_DIM);form_cap(90,24,FORM_OCT[clamp(t->p[P_AOCT],0,3)],T_ACCENT);
  labs[0]="PERCUSSION";labs[1]="DETUNE";labs[2]="LEVEL";val[0]=hasdrums?t->p[P_KEY]:0;val[1]=t->p[P_DETUNE];val[2]=t->p[P_LEVEL];
 }else{
  form_cap(8,4,"TIMBRE",T_TEXT);form_cap(8,24,"A",T_ACCENT);form_cap(26,24,FORM_DOMAIN[da],T_ACCENT);form_cap(132,24,"B",T_THEME);form_cap(150,24,FORM_DOMAIN[db],T_THEME);
  cv_line_t(25,39,105,51,T_ACCENT,1);cv_line_t(205,39,135,51,T_THEME,1);form_cap(109,49,"MIX",T_TEXT);
  labs[0]="DRIVE";labs[1]="TONE";labs[2]="WOBBLE";val[0]=t->p[P_ASWING]*127/100;val[1]=t->p[P_APROB];val[2]=clamp(t->p[P_ARATE],0,7)*127/7;
 }
 for(uint32_t i=0;i<3;i++){int32_t y=72+(int32_t)i*17;form_cap(8,y,labs[i],T_DIM);cv_rect(92,y,140,9,T_RAISE);cv_rect(92,y,clamp(val[i],0,127)*140/127,9,i==0?T_ACCENT:T_THEME);}
 if(layers)form_cap(8,127,hasdrums?"":"NO DRUM PATTERN IN THIS PAIR",T_DIM);
 else{form_cap(8,127,"WOBBLE",T_DIM);form_cap(70,127,FORM_WOB[clamp(t->p[P_ARATE],0,7)],T_TEXT);}
}
static __attribute__((noinline)) void graph_form_lz(const track_t*t){
 uint32_t a=(uint32_t)clamp(t->p[P_E0],0,FORM_N-1),b=(uint32_t)clamp(t->p[P_E1],0,FORM_N-1),m=(uint32_t)clamp(t->p[P_E2],0,127),clock=form_view_clock(t),id=cur_page()->id[0];
 if(id==P_E4)form_rules_panel(t,a,b,m,clock);else if(id==P_LRTRG)form_clock_panel(t,a,b,m,clock);else if(id==P_AORDER)form_notes_panel(t,a,b,m,clock);else if(id==P_CAOS||id==P_KEY)form_layers_panel(t,a,b,m,id==P_KEY);else form_states_panel(t,a,b,m);
}
/* the lens's panel for EDIT's page pg (1), else 0 (its own) */
static int graph_edit_lz(const track_t *t, const page_t *pg)
{
    uint32_t e = trk_eng(t);
    if(e==ENGI_FORM&&(pg->scope==SC_ENGINE||pg->scope==SC_FORM))
        graph_form_lz(t);
    else if ((pg->scope == SC_ENGINE && e != ENGI_DRUM && !(FELUCCA_FM4 && e == ENGI_DIGITAL)) || pg->graph == GR_SWARM)
        graph_sound_lz(t);
    else if (pg->id[0] == P_FTYPE)
        graph_filter_lz(t, pg);
    else if (pg->id[0] == P_VOICE || pg->id[0] == P_ALLOC)
        graph_voice_lz(t, pg);
    else if (pg->graph == GR_FOCUS)
        graph_focus_lz(t);
    else
        return 0;
    return 1;
}
