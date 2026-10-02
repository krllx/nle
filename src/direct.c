/* Direct rendering of NLE's fixed ANSI_DEFAULT tty operations.
 * libtmt is the compatibility oracle, including its immediate wrap and
 * unsigned cursor arithmetic. It is never called by this renderer. */
#include "direct.h"
#include <string.h>
#define ALL_ROWS ((1u << D_ROWS) - 1)
static void clear_range(direct_screen *s, size_t start, size_t end)
{
    if (start >= end) return;
    memset(s->chars + start, ' ', end - start);
    memset(s->colors + start, 0, end - start);
    for (size_t i = start / D_COLS; i <= (end - 1) / D_COLS; ++i)
        s->dirty |= 1u << i;
}
static void scroll_up(direct_screen *s)
{
    memmove(s->chars, s->chars + D_COLS, (D_ROWS - 1) * D_COLS);
    memmove(s->colors, s->colors + D_COLS, (D_ROWS - 1) * D_COLS);
    clear_range(s, (D_ROWS - 1) * D_COLS, D_ROWS * D_COLS);
    s->dirty = ALL_ROWS;
}
void direct_init(direct_screen *s)
{
    memset(s, 0, sizeof *s);
    /* tmt_open uses calloc attrs (fg = 0), before TI resets them. */
    clear_range(s, 0, D_ROWS * D_COLS);
}
void direct_apply(direct_screen *s, enum direct_op op, int a, int b)
{
    s->pending = 1;
    switch (op) {
    case D_MOVE:
        s->col = (size_t)a < D_COLS ? (size_t)a : D_COLS - 1;
        s->row = (size_t)b < D_ROWS ? (size_t)b : D_ROWS - 1;
        break;
    case D_UP:
        /* libtmt MAX casts to size_t: underflow clamps to the last row. */
        s->row = s->row ? s->row - 1 : D_ROWS - 1;
        break;
    case D_DOWN: if (s->row < D_ROWS - 1) ++s->row; break;
    case D_RIGHT: if (s->col < D_COLS - 1) ++s->col; break;
    case D_LEFT: if (s->col) --s->col; break;
    case D_HOME: s->row = s->col = 0; break;
    case D_CLEAR: clear_range(s, 0, D_ROWS * D_COLS); break;
    case D_EOL: clear_range(s, s->row * D_COLS + s->col, (s->row + 1) * D_COLS); break;
    case D_EOS: clear_range(s, s->row * D_COLS + s->col, D_ROWS * D_COLS); break;
    case D_RESET_ATTR: s->fg = -1; s->bold = s->reverse = 0; break;
    case D_BOLD: s->bold = 1; break;
    case D_UNDERLINE: break; /* not represented in tty_colors */
    case D_REVERSE: s->reverse = 1; break;
    case D_COLOR:
        if (a == 7 || a == 8) break; /* gray / NO_COLOR: empty sequence */
        if (a == 0) {
            /* Default substitutes blue for black; darkgray uses bold black. */
            a = b ? 8 : 4;
        }
        /* Low colors emit SGR 0 then 3n. Bright colors emit SGR 1 then 3n.
         * Preserve libtmt's SGR semantics: default fg ignores bold. */
        if (a < 8) { s->bold = s->reverse = 0; }
        else s->bold = 1;
        s->fg = (a & 7) + 1;
        break;
    case D_NOP: break;
    }
}
void direct_char(direct_screen *s, int c)
{
    s->pending = 1;
    /* NLE buffers char before passing it to libtmt. The byte (including
     * the high bit of IBM graphics) is retained in the observation. */
    switch ((unsigned char)c) {
    case 0: case 7: case 14: case 15: return;
    case 8: if (s->col) --s->col; return;
    case 9: if (s->col < D_COLS - 1) do { ++s->col; } while (s->col < D_COLS - 1 && s->col % 8); return;
    case 10: if (s->row < D_ROWS - 1) ++s->row; else scroll_up(s); return;
    case 13: s->col = 0; return;
    }
    size_t i = s->row * D_COLS + s->col;
    s->chars[i] = (unsigned char)c;
    int color = s->fg == -1 ? (c == ' ' ? 0 : 7) : (s->fg - 1) | (s->bold ? 8 : 0);
    s->colors[i] = color + (s->reverse ? 16 : 0);
    s->dirty |= 1u << s->row;
    if (++s->col == D_COLS) {
        s->col = 0;
        if (++s->row == D_ROWS) { --s->row; scroll_up(s); }
    }
}
