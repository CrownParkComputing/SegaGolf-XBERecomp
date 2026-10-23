#!/usr/bin/env python3
"""Build Sega Golf CPU diagnostics with the included Chihiro SDK."""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess
from build_support import ROOT, verify_image

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--recompiler", type=Path)
    parser.add_argument("--generate-only", action="store_true")
    args = parser.parse_args()
    verify_image()
    recompiler = (args.recompiler or ROOT / "out/build/linux-release/xbe_recomp").resolve()
    if not recompiler.is_file():
        parser.error("Build the port's tools first: python3 tools/build_native.py --tools-only")
    fingerprint = hashlib.sha256()
    for path in [ROOT / "assets/golf.xbe", recompiler, *sorted((ROOT / "config").glob("*.toml"))]:
        with path.open("rb") as source:
            fingerprint.update(hashlib.file_digest(source, "sha256").digest())
    stamp = ROOT / ".local/headless-codegen.sha256"
    digest = fingerprint.hexdigest()
    mapping = ROOT / "generated/game/x86_func_mapping.cpp"
    if not mapping.exists() or not stamp.exists() or stamp.read_text().strip() != digest:
        subprocess.run([str(recompiler), "config/game.toml"], cwd=ROOT, check=True)
        stamp.parent.mkdir(parents=True, exist_ok=True)
        stamp.write_text(digest + "\n")
    if not args.generate_only:
        environment = os.environ.copy()
        environment["JOBS"] = str(max(1, args.jobs))
        subprocess.run(["bash", "sdk/harness/build.sh", "-r", str(ROOT), "-g", str(ROOT / "generated/game"),
                        "-o", str(ROOT / "out/harness"), "-n", "SegaGolf", "-x",
                        "/gpu/vk_|/gpu/vulkan|/audio/sdl_audio.cpp|/core/keyboard_pad.cpp"],
                       cwd=ROOT, env=environment, check=True)
