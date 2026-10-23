# Sega Golf Club native recompilation

Standalone experimental Sega Golf Club Network ProTour port, with its patched
XBERecomp runtime included. The supported local disc is GDX-0010B, V3.000,
dated 2004-10-14.
Native Linux boots the original server and satellite over a local link and
reaches Card Entry, character selection and the first hole's 3D course. Mouse
clicks and dragging operate the touch panel directly; right-mouse dragging
operates the cabinet's analog golf club. Controller and keyboard adapters
remain available. A mouse-driven shot has been verified through ball movement
and the next shot. A complete hole, sound fidelity and Android remain
unverified.

Requires Linux x86-64, Python 3.11+, PyCryptodome for disc extraction, Clang,
LLD, CMake 3.25+, Ninja and Vulkan 1.3 development files/drivers. CMake uses an
installed SDL3 3.2+ or fetches SDL3; it also fetches the other pinned build
dependencies and the hash-checked Linux DXC distribution. See
[UPSTREAM.md](UPSTREAM.md) for runtime provenance and notices.

From this repository:

```sh
python3 tools/prepare_game.py "/path/to/Sega Golf Club - Network ProTour v3.000 (2004)(Sega)(JP).zip"
python3 tools/build_native.py --jobs 4 --tests
./run.sh
```

The launcher opens two playable windows, Player 1 (main satellite,
`127.0.0.2`) and Player 2 (sub satellite, `127.0.0.3`). A third process runs
the original server at `127.0.0.1` in the background, using offscreen Vulkan
so it needs no visible game screen. Closing either player window stops all
three processes. Settings, logs and guest HDDs are separate under
`.local/linked/server/`, `.local/linked/main/` and `.local/linked/sub/`.
Use `./run.sh --players 1` for one playable cabinet, or `--show-server` to
display the server for diagnostics. The original server takes
about a minute to initialize; allow roughly 90 seconds for network discovery,
login and title startup. The link uses real TCP/UDP on loopback, including the
game's original discovery and login packets. External online services are
unavailable. A single direct executable launch retains the factory satellite
sub role and cannot supply the server it needs.

Click the player window you want to control. Each cabinet has its own coin
counter, touch panel and swing input. Start both players to use the game's
original local matchmaking; a full head-to-head round remains unverified.

Use either player window for play:

| Action | Mouse | Keyboard | Controller |
| --- | --- | --- | --- |
| Point at the touch panel | Move pointer | Arrow keys | Left stick or D-pad |
| Touch/hold, release to lift | Left-click / drag | Z / Space / W | A / right trigger |
| Backswing | Hold right button and pull down | Hold K (full power) | Pull right stick down |
| Forward swing | Push up while holding right button, or release | Release K | Return right stick |
| Fine cursor movement | — | Q / left Shift | Left trigger |
| Insert coin | — | Backspace | Back |
| Start | — | Enter | Start |
| Settings/resume | — | F10 | R3 |

The mouse points directly at the game image; clicks in the letterbox borders
are ignored. The system pointer replaces the virtual cursor during mouse use.
Keyboard/controller input retains a cyan cursor, orange when pressed. Touch input goes
through the original serial packet parser, including calibration, press/hold/
release states and checksums. Input releases when the window loses focus or
settings open. Keyboard bindings can be changed in the satellite's
`settings.toml`. Escape also closes the settings panel.

To swing, hold the right mouse button near the upper half of the game image
and pull down. About one third of the image height gives a full backswing;
shorter pulls select less power. Pause briefly at the desired backswing, then
push upwards to strike. Letting go also returns the club to its rest position.
This drives the original club sensor, power filter and swing timing through
JVS analog channel 3. Left-click remains the touch-panel control.

After startup, insert a coin with Backspace and left-click the title
screen. This adds a credit and enters Card Entry; the no-card option is on the
left. A complete hole has not yet been validated.

Preparation requires PyCryptodome. It reads the zipped GDI/raw-sector dump,
extracts the 512 MB container, verifies the track's ZIP CRC, decrypts FATX and
writes the original game tree under `assets/`. It refuses to overwrite previous
outputs. Private manifests, containers, generated code and builds are ignored.
The build checks the exact `golf.xbe` SHA-256 before translating it.

The key is publicly documented in the
[Citylan dump notes](https://www.citylan.it/index.php/Sega_Golf_Club_Network_Pro_Tour_2005_(Rev_C)_(GDX-0010C)).
[MAME's Chihiro driver](https://github.com/mamedev/mame/blob/master/src/mame/sega/chihiro.cpp)
identifies the same security PIC for revisions B and C. Successful FATX
decryption was verified against the local revision B archive.

For CPU diagnostics, first build the native tools, then run from this port:

```sh
python3 tools/build_headless.py --jobs 4
timeout 30s ./out/harness/SegaGolf "$PWD/assets"
```

The headless renderer does not complete occlusion-query reports. The game
polls an empty query during graphics initialization before its first present,
so native Vulkan is required to progress past that point. Cabinet EEPROM,
backup memory and controller firmware buffers currently last only for the
running process. `golf_test.xbe` is extracted but has no translated target.

See [PORT_STATUS.md](PORT_STATUS.md) for identity, evidence and open work.

Validation from this port:

```sh
ctest --test-dir out/build/linux-release --output-on-failure
SEGAGOLF_TRACE_STARTUP=1 python3 tools/run_linked.py --seconds 150
```

Socket tests need permission to create local sockets. They verify real TCP/UDP
payload exchange, BSD socket addresses, asynchronous completion, DMA transfers
and readiness polling. Touch tests check cursor movement, packet checksums,
controller focus handling and the original serial command ABI. Swing tests
send SDL mouse gestures through the original analog decoder and swing filter,
checking backswing latching, forward strokes and the right-stick fallback.
An optional `-DSEGAGOLF_INPUT_SMOKE=ON` build injects a bounded sequence of SDL
mouse and keyboard events after 100 seconds; it is disabled in normal builds.

For an existing dependency cache, `tools/build_native.py` accepts
`--deps-dir /path/to/third_party` (containing `zydis-src`, `imgui-src`, etc.)
and `--dxc-dir /path/to/extracted/dxc`. `DXC_SOURCE` is also supported.
`--tools-only` builds the recompiler without game files; `--generate-only`
regenerates the private guest C++ without building the native game.
