#ifndef NLEGO_TTY_API_V1_H
#define NLEGO_TTY_API_V1_H
/* Optional exports in patched libnethack.so. Resolve with dlsym; do not link
 * directly, and do not add fields to nle_obs / nle_settings. */
enum nle_tty_mode_v1 { NLE_TTY_STOCK = 0, NLE_TTY_DIRECT = 1, NLE_TTY_SHADOW = 2 };
/* Set before nle_start. Returns 0 on success, -1 for active game / bad mode. */
int nle_tty_set_mode_v1(int mode);
int nle_tty_mode_v1(void);
void nle_tty_stats_v1(unsigned long long *comparisons, unsigned long long *mismatches);
#endif
