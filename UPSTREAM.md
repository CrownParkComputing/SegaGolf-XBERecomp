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

`patches/preview-depth.patch` preserves a larger shared depth buffer when a
smaller character-preview target is bound, with a Vulkan regression covering
occlusion and retained depth contents. This patch is already applied too.

`UPSTREAM.json` records the original suite commit, patch list and SHA-256 of
each included SDK file. The SDK's original license and third-party notices
are retained in `sdk/LICENSE` and `sdk/THIRD_PARTY.txt`.

The standalone copy omits the generic `sdk/templates/game/` scaffolding,
`sdk/tools/sdk_sync/` multi-repository maintenance tools and the cookbook
pointer that depended on the suite's directory layout. The SDK README links
to the original files at the pinned upstream commit. `UPSTREAM.json` lists
these omissions and records the updated README hash.

The SDK identifies its original upstream as
[TGP482/XboxXBE-Recomp](https://github.com/TGP482/XboxXBE-Recomp).
External build dependencies retain their upstream notices and are fetched at
the versions in `sdk/cmake/XbeRecomp.cmake`; the Linux DXC archive is hash-pinned.

Game assets, generated game code, local disc containers, build outputs, saves,
settings and logs are excluded from this repository. Selected cropped
screenshots are included under `docs/images/`; diagnostic captures remain local.
