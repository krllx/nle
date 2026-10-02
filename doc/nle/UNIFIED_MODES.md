# One NLE library: renderer and structured UI

This fork owns both the direct tty renderer (`src/direct.c`, `include/direct.h`)
and the visible structured UI (`src/nle_ui.c`, `include/nle_ui.h`). Both are
compiled into `libnethack.so` by the ordinary CMake nethack target. The original
NLE renderer stays available. No post-export source patches are needed.

The original `nle_obs` and `nle_settings` ABI is unchanged. Optional symbols:

- `nle_tty_set_mode_v1(int)` before nle_start: 0 stock, 1 direct, 2 shadow.
  Invalid values or configuring an active game return -1.
- `nle_tty_mode_v1()` returns the requested mode, including when screen is off.
- `nle_tty_stats_v1()` reports comparisons/mismatches for this game. Shadow
  aborts on a differing cell/color/cursor. Each new game resets both counters.
- `nle_ui_configure_v3(flags)` and `nle_ui_current_v3()` keep their v3 ABI:
  enabled=1, screen=2, positioned diagnostics=4. Default is screen only.

Renderer mode and UI flags are independent. Screen enabled permits any renderer
with or without the structured channel. Screen disabled requires the structured
channel and skips all three renderers: no tmt allocation/parsing, no direct grid
updates/publication, no ANSI buffering/coordinate formatting. A shadow request
with screen off performs zero comparisons and does not verify rendering.
The tty controller still determines wrapping, visible pages and input stops.
UI character/move/style/clear events happen before output suppression.

Renderer and producer state are library-private, following NLE's one game per
private library copy model. No environment variable chooses the native modes.
Clients must load private copies to isolate games, as upstream pynethack does.
One library copy cannot host multiple simultaneous NetHack games. The nle-go
loader freezes Config.Renderer at engine creation and loads a memfd copy for each
engine; NLE_TTY_MODE is a compatibility default resolved once in Go.

Ttyrec needs stock rendering and screen enabled. Non-stock/no-screen ttyrec
requests fail explicitly. Raw terminal capabilities outside the supported
ANSI_DEFAULT operation path fail explicitly in direct/shadow mode.

Reproducible standalone renderer oracle test (UBSan, libtmt independent):

```sh
cc -std=c11 -O2 -fsanitize=undefined -fno-sanitize-recover=all \
  -Iinclude -Ithird_party/libtmt tests/direct_tty_test.c \
  src/direct.c third_party/libtmt/tmt.c -o direct_tty_test
./direct_tty_test
```

Combined arrays, controller/converter, bot actions, cuts and multi-engine tests
are owned by nle-go `tools/unified-check.sh`, `tools/unified-cuts.py`,
`tools/unified-fault.py` and `engine/stock/renderer_test.go`. Original/pristine
reference libraries and corpus must stay independent and unchanged.

Future checkpoints must retain the requested renderer/UI flags, active input
controller state, direct grid/attributes/dirty and pending masks, tmt state and
unflushed output when present, UI generation/buffers/visible spans and diagnostics,
and the suspended NetHack execution context. Existing bot snapshot tests replay
the engine prefix; they do not restore this native state directly.
