/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (JIANT BYTE, phase 8) the formula sequencer: ARP's E: modes (seq.c ARP_EQ, arp_eq through arp_pick): each step's place
 * in the held notes the equation's, modulo their number; E:3X with four notes 0 3 2 1 ..; E:TAB its table; every place
 * in range, the same step the same place; the eight modes eight different lines
 *   cc -O2 -Ibuild/gen -Ifirmware/src -o build/host/arp_eq_test tests/arp_eq_test.c -lm */
#include <stdarg.h>
#define main hostsim_main
#include "hostsim.c"
#undef main
static int fails;
static void ok(int c, const char *fmt, ...)
{
    va_list ap; char b[300];
    va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    printf("arpeq: %-100s %s\n", b, c ? "ok" : "FAIL");
    if (!c) fails++;
}
static uint32_t pick(uint32_t mode, uint32_t i, uint32_t len)
{
    static uint32_t list[64];
    uint32_t k;
    for (k = 0; k < len; k++) list[k] = 48u + k * 3u;
    trk[0].p[P_AMODE] = (int16_t)mode;
    trk[0].arp_idx = (uint16_t)i;
    (void)arp_pick(&trk[0], list, len);              /* (c1fault: the mode's equation asked for, the main loop's */
    fvm_host_serve();                                /* fvm_serve compiles it) */
    return arp_pick(&trk[0], list, len);
}
int main(int argc, char **argv)
{
    static uint32_t line[ARP_NEQ][32];
    uint32_t m, i, good = 1, inr = 1, same = 1, distinct = 0;
    (void)argc; (void)argv;
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    for (i = 0; i < 16u; i++) good &= pick(AM_EQ0, i, 4u) == (i * 3u) % 4u;
    ok(good, "E:3X with four notes: 0 3 2 1 0 3 .. (s * 3 mod 4)");
    good = 1;
    for (i = 0; i < 16u; i++) good &= pick(AM_EQ0 + 4u, i, 4u) == (("0213"[i & 3u] - 48u + (i >> 2)) % 4u);
    ok(good, "E:TAB: its table, shifted each bar of four");
    for (m = 0; m < ARP_NEQ; m++)
        for (i = 0; i < 32u; i++) {
            uint32_t a = pick(AM_EQ0 + m, i, 5u);
            inr &= a < 5u;
            line[m][i] = a;
        }
    for (m = 0; m < ARP_NEQ; m++)
        for (i = 0; i < 32u; i++) same &= pick(AM_EQ0 + m, i, 5u) == line[m][i];
    ok(inr && same, "every mode: each place among the five notes, the same step the same place (asked again)");
    for (m = 0; m < ARP_NEQ; m++) {
        uint32_t j;
        for (j = 0; j < m && memcmp(line[j], line[m], sizeof line[m]); j++) {}
        distinct += j == m;
    }
    ok(distinct == ARP_NEQ, "the %u modes %u different lines (32 steps, five notes)", (unsigned)ARP_NEQ, distinct);
    printf(fails ? "arpeq: %d FAILED\n" : "arpeq: all ok\n", fails);
    return fails != 0;
}
