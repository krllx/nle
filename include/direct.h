#ifndef NLE_DIRECT_H
#define NLE_DIRECT_H
#include <stddef.h>
#include <stdint.h>
#define D_ROWS 24
#define D_COLS 80
/* Operations describe terminal effects, before ANSI serialization. */
enum direct_op { D_MOVE, D_UP, D_DOWN, D_RIGHT, D_LEFT, D_HOME,
    D_CLEAR, D_EOL, D_EOS, D_RESET_ATTR, D_BOLD, D_UNDERLINE,
    D_REVERSE, D_COLOR, D_NOP };
typedef struct direct_screen {
    unsigned char chars[D_ROWS * D_COLS];
    signed char colors[D_ROWS * D_COLS];
    uint32_t dirty;
    size_t row, col;
    int fg, bold, reverse;
    int pending;
} direct_screen;
void direct_init(direct_screen *);
void direct_apply(direct_screen *, enum direct_op, int, int);
void direct_char(direct_screen *, int);
#endif
