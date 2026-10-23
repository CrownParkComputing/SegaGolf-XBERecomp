# Runtime provenance

This repository contains one game port: Sega Golf Club Network ProTour
GDX-0010B V3.000. Its recompiler and host runtime are included under `sdk/`.
No separate suite checkout or SDK symlink is needed.

The runtime snapshot comes from
[CrownParkComputing/xbox-recomp-suite](https://github.com/CrownParkComputing/xbox-recomp-suite)
at commit `81b5264735ce97cf3c88ae6cdd63aee3f406215d`, with the Chihiro patches
under `patches/chihiro/` already applied. They cover 128 MiB memory, visibility
report completion, audio buffer handling, indexed draws, controller capability
layout, target alpha, texture ownership/binding and offscreen Vulkan for the
network server. Do not apply these patches again to the included SDK.

`patches/english-ui-observers.patch` records the additional renderer observation
hooks used by the English label layer. Those are also already applied.

`UPSTREAM.json` records the original suite commit, patch list and SHA-256 of
each included SDK file. The SDK's original license and third-party notices
are retained in `sdk/LICENSE` and `sdk/THIRD_PARTY.txt`.

The SDK identifies its original upstream as
[TGP482/XboxXBE-Recomp](https://github.com/TGP482/XboxXBE-Recomp).
External build dependencies retain their upstream notices and are fetched at
the versions in `sdk/cmake/XbeRecomp.cmake`; the Linux DXC archive is hash-pinned.

Game assets, generated game code, local disc containers, build outputs, saves,
settings, logs and captures are excluded from this repository.
