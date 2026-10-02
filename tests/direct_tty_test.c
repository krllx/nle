#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "direct.h"
#include "tmt.h"
static int same(direct_screen *d, TMT *t)
{
    const TMTSCREEN *s = tmt_screen(t);
    const TMTPOINT *p = tmt_cursor(t);
    if (p->r != d->row || p->c != d->col) return 0;
    for (size_t i = 0; i < D_ROWS * D_COLS; ++i) {
        TMTCHAR c = s->lines[i / D_COLS]->chars[i % D_COLS];
        int color = c.a.fg == TMT_COLOR_DEFAULT ? (c.c == ' ' ? 0 : 7) :
            (c.a.fg - TMT_COLOR_BLACK) | (c.a.bold ? 8 : 0);
        color += c.a.reverse ? 16 : 0;
        if ((unsigned char)c.c != d->chars[i] || color != d->colors[i]) {
            fprintf(stderr, "cell %zu: %u/%u %d/%d\n", i,
                    (unsigned char)c.c, d->chars[i], color, d->colors[i]);
            return 0;
        }
    }
    return 1;
}
static void op(direct_screen *d, TMT *t, enum direct_op cmd, int a, int b, const char *seq)
{
    direct_apply(d, cmd, a, b);
    tmt_write(t, seq, strlen(seq));
    assert(same(d, t));
}
static void byte(direct_screen *d, TMT *t, int c)
{
    char v = c;
    direct_char(d, c);
    tmt_write(t, &v, 1);
    assert(same(d, t));
}
static unsigned rng = 0xdeadbeef;
static unsigned next(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
int main(void)
{
    direct_screen d;
    direct_init(&d);
    TMT *t = tmt_open(D_ROWS, D_COLS, NULL, NULL, NULL, true);
    assert(t && same(&d, t));
    op(&d, t, D_RESET_ATTR, 0, 0, "\033[0m");
    /* Boundary controls: bottom-right wrap, LF scroll, TAB at right edge,
     * UP at the top (libtmt unsigned arithmetic) and LEFT at column zero. */
    op(&d, t, D_MOVE, 79, 23, "\033[24;80H");
    byte(&d, t, 'X');
    byte(&d, t, '\n');
    op(&d, t, D_MOVE, 79, 0, "\033[1;80H");
    byte(&d, t, '\t'); byte(&d, t, 'Z');
    op(&d, t, D_HOME, 0, 0, "\033[H");
    op(&d, t, D_UP, 0, 0, "\033[A");
    op(&d, t, D_LEFT, 0, 0, "\033[D");
    /* Representative dialogs: pagination marker, a selected menu row,
     * and text input with backspaces, CR and redraw under active attributes. */
    op(&d,t,D_CLEAR,0,0,"\033[2J");
    op(&d,t,D_HOME,0,0,"\033[H");
    const char *pages[] = {"Pick an item:", "\r\n a - a blessed +1 dagger",
                         "\r\n b - 3 potions", "\r\n--More--"};
    for (size_t n = 0; n < sizeof pages / sizeof pages[0]; ++n)
        for (const char *p = pages[n]; *p; ++p) byte(&d,t,*p);
    op(&d,t,D_REVERSE,0,0,"\033[7m");
    for (const char *p = "selected"; *p; ++p) byte(&d,t,*p);
    op(&d,t,D_RESET_ATTR,0,0,"\033[0m");
    for (const char *p = "\rName? abc\b\bde"; *p; ++p) byte(&d,t,*p);
    op(&d,t,D_EOL,0,0,"\033[K");
    /* Deliberate corruption must be visible to the independent oracle. */
    d.chars[10] ^= 1; assert(!same(&d, t)); d.chars[10] ^= 1;
    d.colors[10] ^= 1; assert(!same(&d, t)); d.colors[10] ^= 1;
    d.col ^= 1; assert(!same(&d, t)); d.col ^= 1;
    for (int i = 0; i < 20000; ++i) {
        char seq[64];
        switch (next() % 17) {
        case 0: {
            int x = next() % 85, y = next() % 28;
            snprintf(seq, sizeof seq, "\033[%d;%dH", y + 1, x + 1);
            op(&d,t,D_MOVE,x,y,seq); break;
        }
        case 1: op(&d,t,D_UP,0,0,"\033[A"); break;
        case 2: op(&d,t,D_DOWN,0,0,"\033[B"); break;
        case 3: op(&d,t,D_RIGHT,0,0,"\033[C"); break;
        case 4: op(&d,t,D_LEFT,0,0,"\033[D"); break;
        case 5: op(&d,t,D_EOL,0,0,"\033[K"); break;
        case 6: op(&d,t,D_CLEAR,0,0,"\033[2J"); break;
        case 7: op(&d,t,D_HOME,0,0,"\033[H"); break;
        case 8: op(&d,t,D_BOLD,0,0,"\033[1m"); break;
        case 9: op(&d,t,D_RESET_ATTR,0,0,"\033[0m"); break;
        case 10: op(&d,t,D_REVERSE,0,0,"\033[7m"); break;
        case 11: op(&d,t,D_UNDERLINE,0,0,"\033[4m"); break;
        case 12: {
            int c = next() % 16, dark = next() % 2;
            if (c == 7 || c == 8) strcpy(seq, "");
            else if (!c) strcpy(seq, dark ? "\033[1;30m" : "\033[0;34m");
            else snprintf(seq,sizeof seq,"\033[%d;3%dm",c >= 8,c & 7);
            if (*seq) op(&d,t,D_COLOR,c,dark,seq);
            else { direct_apply(&d,D_COLOR,c,dark); assert(same(&d,t)); }
            break;
        }
        case 13: op(&d,t,D_EOS,0,0,"\033[J"); break;
        case 14: {
            static const char controls[] = {0,7,8,9,10,13,14,15};
            byte(&d,t,controls[next() % sizeof controls]); break;
        }
        default: byte(&d,t,32 + next() % 224); break;
        }
    }
    tmt_close(t);
    puts("direct tty: targeted edges, intentional corruption, 20000 randomized operations passed");
    return 0;
}
