# Binary UI v3 fork

Modified 2026-10-01. Base: NetHack-LE/nle tag v1.3.0,
commit 70cb9b5260d05b38ee1ee1b0228d5f6f8ca54655, NetHack 3.6.7.
This is the NLE-integrated game used by the pinned NLE 1.3.0 runner.
It retains the upstream NetHack General Public License and original copyright notices.

The C sources in this branch are the authoritative implementation. Source changes are
made here, committed here and consumed by a pinned fork commit in the Go runner.
There is no required patch application when building this branch.

## Build

Use the ordinary upstream CMake toolchain (C/C++ compiler, flex/bison, Python development
headers, CMake >=3.28 and upstream build dependencies). Build only the game library:

```sh
tools/build-binary-ui.sh
```

Outputs: build/binary-ui/libnethack.so and build/binary-ui/nethackdir.
CMake arguments can follow the script; BINARY_UI_BUILD_DIR selects another directory.
For offline builds, provide the same pinned bzip2/pybind11/deboost.context source directories
through FETCHCONTENT_SOURCE_DIR_* and FETCHCONTENT_FULLY_DISCONNECTED=ON as upstream does.
The standard Python build remains available and defaults to the unchanged rendered path.

## Interface

Canonical ABI: include/nle_ui.h. Optional symbols nle_ui_configure_v3(uint32_t flags)
and nle_ui_current_v3(void), discovered explicitly by consumers. Linux amd64 header
192 bytes, menu record 40, text/draw 28. nle_obs and nle_settings are unchanged.
Configuration bits: 1 channel, 2 stock screen, 4 diagnostic native display operations.
Configure before nle_start; flags 0 are invalid. Current pointers are C-owned and valid
until step/reset/end. Copy retained data into consumer-owned memory.

Every raw yield has a generation. Export current visible menu rows and selection,
text spans actually emitted and surviving native clear/scroll, visible prompts/choices,
line/numeric editing, coordinates, native input wait and already-shown messages.
No queued/future pages, internal item identifiers or hidden accelerators are exposed.
Display regions come from the original tty controller; no terminal rows are parsed to
recover questions or menus. legacy_flags explicitly preserve visible answered [yn]
labels and literal false-rumor More tokens while native wait remains truthful.

No-screen disables tmt allocation/update and ANSI buffering/parsing, keeping original
tty interaction/wrapping/keypress stops. Diagnostics are optional and separate from
bot decisions; a converter consumes native positioned Put/Clear/Scroll operations.
Feature 256 advertises the compatibility indicator flags. Clients must require v3
and exact header size; old v1/v2 UI channels are not used by this fork.

## Validation

Transferred from the verified Go runner producer d3e4c217eb31e0f8ec6a29dcb57d3061b388683c.
The modified source bodies match that producer; only modification notices were added.
Stock reference: pinned NLE 1.3.0 arena library, time()=1700000000, identical seeds/options.
Its final 59-file corpus replay checked 2300775 actions, all 14 rendered observations,
non-screen arrays with drawing disabled, done/control/metrics and on/off UI entities,
with zero differences. Direct-input/visible-page/lifetime tests and independent full
screen converter also passed. Go CPU and allocation profiles are in the runner report.
A fresh build/replay from this source branch is verified again before publication.

## Maintenance

Keep this branch based on v1.3.0 until an explicit compatibility upgrade is validated.
Make engine/window/ABI changes directly in src/, win/tty/ and include/. Keep the ABI
versioned, preserve independent stock rendering and visible-information boundaries.
Pin downstream consumers to a concrete commit; after changes, compare against the
independent original library, not only against this fork's own converter.

Do not replace this base with upstream main or plain NetHack without validating NLE
observations, getpos, menu/message stops, full bot action traces and restore behavior.
Upstream game/Python integration documentation follows in the original README.
