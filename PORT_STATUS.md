# Sega Golf status — 23 October 2026

This standalone repository packages the `SegaGolf` port from the combined
source suite, with its patched SDK included as source. Linux native
and CPU diagnostic builds succeed. Two native instances run the original
server and main satellite over loopback TCP/UDP. Both complete network matching;
the satellite enters its original warning, title and attract modes. Inserting a
coin adds one credit, and touching the title starts the game and reaches Card
Entry, character selection and the first hole. The final image retains the 3D
course and golfer alongside the HUD. A right-mouse swing produces a rendered
shot, ball movement and the next-shot state. A complete hole has not yet been validated.

## Matching input

- Archive: `Sega Golf Club - Network ProTour v3.000 (2004)(Sega)(JP).zip`.
- Boot metadata: GDX-0010B, V3.000, 20041014.
- Preparation selects `GOLF_512.BIN`; the archive also contains a 1 GB variant.
- Extracted 1,449 files, 433,863,433 bytes. All extracted sizes and SHA-256
  hashes were independently rechecked against `.local/extracted.json`.
- `golf.xbe`: 2,818,048 bytes, SHA-256
  `00dd5d68d50104d67e23965c36245a315732d239228e3c8ccf10f5a75b8fab0d`.
- Image base `0x10000`, debug-key entry `0xE92E7`, title ID zero, XDK 5788.
- `golf_test.xbe` is retained as an original asset and is not translated.

## Runtime integration

`config/xdk-5788.toml` records device, stream, controller and effects-image
offsets verified in this executable. Each entry includes instruction evidence.
XbSymbolDatabase supplied symbol candidates; verified public sound wrapper
addresses resolve its alternative signature names and macro-defined handlers.
Translation currently discovers approximately 17,479 functions, including the
explicit CRT thread-notify callback at `0x183CC4`. Unsupported instructions and
overlapping decodes on unvisited paths remain; translation is experimental.

The included SDK already contains the recorded Chihiro patches in
`patches/chihiro/`; no suite checkout is needed to build. The original suite's
preparation previously invoked
`git apply` from an ignored SDK subdirectory, which could report success while
skipping the patch paths. The preparation helper now anchors patches to the
derived SDK explicitly and invalidates the older cache format. The shared
retail SDK stays unchanged; Sega Golf's derived memory size is verified as
128 MiB. A nested-repository regression tests that the overlay changes the
derived copy and leaves the shared source unchanged.

The game-specific scheduling bridge retains the process quantum property
without dereferencing an absent Xbox KPROCESS. Host scheduling remains an
approximation. Debugger output is logged through the verified XDK wrapper.
JVS storage boundaries provide erased EEPROM, backup SRAM and controller
firmware buffers, plus a BCD RTC using host UTC. The original acLib
initialization and storage processing continue. These buffers are volatile.

The virtual cabinet supplies JVS capabilities, switches, coin counters, analog
reports and successful report-transfer statistics. The physical discovery
boundary publishes baseboard readiness and the network protocol version 12.07
required by the original check at `0x8F9D3..0x8F9FA`. The original checks and
startup tasks remain active.

Touch-panel emulation answers the game's reset, baud, mode, calibration and
delay serial commands. Keyboard/controller cursor movement generates 10-byte
contact packets with original checksums and 12-bit coordinates; the original
parser at `0xC6A90` receives down, hold and release states. Physical Y reversal
is preserved. The mouse maps directly to the letterboxed game image, with
left-click and drag feeding the original serial protocol. Press/release edges
are queued at the SDL event boundary so quick clicks survive between serial
polls. Border clicks are ignored, and focus loss, leaving the window or opening
settings releases touch. The system pointer is shown during mouse use;
keyboard/controller input retains the virtual cursor. The F10/R3 settings
panel uses SDL/ImGui.

The club sensor is JVS analog channel 3, not the two axes used by the virtual
touch cursor. `0x14B210` converts its wire bytes to a host-order word;
`0x834A0` copies the array to the input record and `0x82CA0` selects channel 3
into input context `+0xB4`. Previously this was fixed at 32768. Right-mouse
dragging now supplies 0 at rest and up to 45000 at full backswing; pulling down
raises the club, and pushing up or releasing returns it. Right-stick down and
return, or K and release, supply the same sensor. The original `0x836B0`
ten-sample filter, power latch, forward detection and gameplay logic remain
active. Startup calibration confirms start=15000, rest=8000 and full=45000.

The original visibility-result reader remains active. Its wrapper drains guest
push commands and waits for real Vulkan reports, allowing the two empty startup
queries to complete before the first present. CPU diagnostics stop here because
the headless backend has no report implementation.

## Local cabinet link

`run.sh` starts `tools/run_linked.py`, which launches a server (operator mode 2,
ID 0, `127.0.0.1`), Player 1/main satellite (mode 0, ID 1, `127.0.0.2`) and
Player 2/sub satellite (mode 1, ID 2, `127.0.0.3`). These are
the original operator settings, applied after the game's backup reader. Each
process has a separate executable directory, settings, logs and guest HDD.
Closing either player window stops all three; a launcher lock prevents
duplicate cabinet sets. The server uses the existing offscreen Vulkan renderer
with real visibility reports, and its SDL window is hidden. Only the two
playable windows remain on the desktop. `--players 1` retains the earlier
single-cabinet setup; `--show-server` displays the server for diagnostics.

The two-cabinet setup was verified with the server's SDL window hidden and
offscreen Vulkan active. Both satellites reached zero startup blockers and
matching state 4/4 with connected=1 and empty command queues by 99–104 seconds.
Desktop captures at 10 and 110 seconds show exactly two game windows. Logs and
captures are archived under `.local/evidence/two-cabinets-offscreen-server/`.
Both also joined a lobby during the earlier run with a visible server. Shared
round progression and a complete head-to-head round remain unverified.

The acLib socket adapter implements real nonblocking TCP/UDP, asynchronous
accept/connect/receive, descriptor-index handles, board DMA reads/writes, BSD
socket addresses and readiness polling. Broadcast packets are delivered
unchanged to the virtual loopback cabinet addresses. External addresses fail
as unreachable. No server packets or network-success flags are fabricated.

The original server discovers its environment before opening its TCP listener,
taking roughly 75 seconds. In a bounded linked run, the satellite logged
DISCOVER SERVER, LOGIN SERVER, GET SERVER INFO, JOIN GROUP and SET AP_DATA;
both startup blocker counts reached zero around 80–86 seconds. It entered
MODE-TITLE around 95 seconds and MODE-ADVERTISE afterward. The network wait
does not complete when running only the factory satellite-sub configuration.

## Open work

- Validate a complete hole and a range of shot types/power levels.
- Validate a complete head-to-head round with both playable cabinets.
- Investigate remaining texture lookup warnings.
- Verify sound fidelity and sound wrapper behavior during play; DSP envelopes
  remain unsupported by the runtime.
- Add cabinet persistence and verify restart behavior.
- Five kernel imports remain unimplemented: ObOpenObjectByName,
  IoQueryVolumeInformation, NtCreateIoCompletion, NtSetIoCompletion and
  NtRemoveIoCompletion. They have not been reached on the current boot path.
- Android packaging/device validation, external online services, card
  peripherals and the separate test executable are unverified.

Detailed logs, disassembly, extraction manifests and captures are local under
`.local/` and excluded from source control.

## Validation

Native Vulkan opens the original ADX banks, accepts an effects image and loads
scene/animation data. A 150-second linked run completes startup and reaches the
title/attract tasks, with both processes exiting zero on bounded host stop.
Current paired logs are under `.local/linked/{server,main}/stdout.log`.
The completed input run and captures are archived in
`.local/evidence/linked-input-final/` so normal launches do not replace them.

The linked SDL input run adds a visible credit with Backspace, then sends a
Space press through the original touch parser. This consumes the credit and
enters MODE-GAME / Card Entry. Captures at 108 and 120 seconds confirm the
credit counter and the interactive Card Entry screen. No card device is
emulated. A 155-second capture shows character selection, including the 3D
golfer. F10 opens settings and Escape resumes without losing the local link.
A complete hole remains unverified.

The gameplay black screen was reproduced with frame captures: the original
course and golfer were visible in the scene target but a later composition
quad replaced them with black. Surface getters failed to retain the parent
texture when a device-held surface gained its first public reference. The
original release path then underflowed the parent's public count into its
resource type bits, so the renderer stopped recognizing those textures.
`texture-binding.patch` preserves that parent ownership and captures texture
registers when bound. Subsequent captures retain the course and golfer in the
final image. The combined fix was checked through a mouse-driven tutorial exit
into first-hole gameplay at 260 seconds, with course, golfer, mini-map and HUD
all visible. Logs and images are archived under
`.local/evidence/mouse-gameplay-final/`. Diagnostic evidence is excluded from
source control.

The five native regression tests pass: real TCP connection/accept, exact TCP and
UDP request/reply bytes through board DMA, socket readiness and handle
exhaustion; plus cursor movement, clipping, fine movement, serial handshake,
contact checksums, edge transitions, quick mouse clicks, border mapping, focus
release and stack cleanup. The resource checks exercise the actual host API
entries, including 2,000 surface reacquisitions with balanced parent ownership
and texture bindings that survive edits to the original header.
The swing test sends actual SDL right-button/motion events through the mouse
adapter, then runs the original recompiled byte converter, input decoder and
swing filter. It verifies rest, variable power, backswing latching, one forward
stroke per gesture, release-to-hit, right-stick return, focus handling and
settings suppression. Only hardware node lookup and the cumulative swing counter
are replaced in this test.
An opt-in SDL input smoke build separately verifies real host mouse and keyboard events
reach the original touch parser, rather than injecting guest state. Production
builds disable those automatic events.

A subsequent linked SDL run exercised the mouse swing on the first hole.
The original input context received a variable backswing up to 45000, then
forward=1/rest=1 as the mouse returned. The 276-second capture shows the
golfer following through and a moving ball with DRIVE 34.9y. At 290 seconds
the game has advanced to the next shot with an unplayable-lie penalty. This
verifies a shot and its outcome, rather than a complete hole. Before,
backswing, shot and result captures and both logs are archived under
`.local/evidence/mouse-swing-final/`. Native and CPU diagnostic builds succeed;
the normal native build disables scripted inputs and the swing trace hooks.

The standalone English pass includes 66 runtime messages and 243 sprite labels,
enabled by default and reversible through F10 or `SEGAGOLF_LANGUAGE=ja`.
It covers card entry, common dialogs, play modes, multiplayer, tutorial text,
club/camera/aim/spin controls and selected shot feedback. The generator validates
the original executable, message capacity, printf argument types and XST/XPR
layout. Runtime messages are fingerprint-checked before changing guest memory;
the Japanese originals are retained for switching back. Sprite labels use
renderer observations and host text panels, without modifying game artwork or
touch regions. Counter labels cover only their portion of combined graphics.

Native desktop captures confirm English card-entry controls and messages,
tutorial instructions, and the first-hole gameplay controls. A mouse-driven
shot still reaches the original forward-stroke detector and rendered shot.
The language regression checks all original message fingerprints, exact Japanese
restoration, unchanged surrounding memory, rejection of an altered original,
Python/C++ texture signatures, the renderer's reversed V coordinates and label
bounds that preserve live distance/shot numbers. All five CTest targets pass;
the standalone native and CPU diagnostic builds succeed. Evidence remains local
under `.local/evidence/english-interface-final/` and is excluded from Git.
Course-specific commentary, names, operator text, audio and some HUD artwork
remain Japanese; this is a first translation pass, not a complete localization.

The SDK-overlay regression passes, Python tools compile, extraction refuses to
overwrite existing files, and all 1,449 extracted file hashes/sizes were
independently verified. The source diff passes whitespace checks; ignored game
files and build products are excluded from the new source listing.

The character-selection preview now keeps the main 800x600 depth buffer while
drawing into its 512x512 colour target. The renderer previously dropped depth
attachments whenever their dimensions differed, causing hidden geometry to
draw over faces, hats and clothing. Vulkan allows a larger attachment covering
the render area. Native captures and user verification confirm the preview
is corrected. A focused GPU regression fails against the original condition
and passes with the fix in both dynamic-rendering and render-pass modes; it
also checks depth contents survive changes of colour target.

The launcher offers one or two playable windows with English or Japanese text,
remembers the choice in `.local/launcher.json`, and accepts `--players`,
`--language`, `--menu` and `--no-menu`. With Zenity installed, an argument-free
desktop launch opens the options window. F10 has an interface-language selector
and shows the current window count. Launcher tests run temporary stand-in
executables to check role/IP/language forwarding, process cleanup, saved choices
and cancellation. All eight enabled CTest targets pass; the full SDK diagnostic
suite is opt-in for manual use.

Audio tracing found valid stereo PCM from the original game (44.1 kHz, 16-bit,
with nonzero mixed output), while the laptop's default PipeWire speaker sink
was muted. The sink was unmuted and the temporary tracing was removed from the
normal build. A six-second capture from the speaker-output monitor confirms
48 kHz stereo audio reaches the output device with both playable windows
running. Sound fidelity, effects and full-round audio remain unverified.
Diagnostic logs are archived under `.local/evidence/audio-diagnosis/`.

The README includes cropped native screenshots of card entry, the corrected
character preview, the tutorial and first-hole play, plus partial-translation
coverage, language switching and launcher instructions. These selected images
are tracked under `docs/images/`; full desktop captures remain private.
