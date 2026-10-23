#!/usr/bin/env python3
"""Configure and build the standalone Sega Golf port on Linux."""
import argparse
from pathlib import Path
import subprocess
from build_support import ROOT, dependency_options, verify_image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--debug", action="store_true")
    parser.add_argument("--tools-only", action="store_true")
    parser.add_argument("--generate-only", action="store_true")
    parser.add_argument("--tests", action="store_true", help="Also build the regression tests")
    parser.add_argument("--deps-dir", type=Path, help="Optional cache of dependency source trees")
    parser.add_argument("--dxc-dir", type=Path, help="Optional extracted Linux DXC distribution")
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    if args.tools_only and args.generate_only:
        parser.error("--tools-only and --generate-only are mutually exclusive")
    if args.tests and (args.tools_only or args.generate_only):
        parser.error("--tests requires a full native build")
    try:
        options = dependency_options(args)
        if not args.tools_only:
            verify_image()
    except (OSError, ValueError) as error:
        parser.error(str(error))
    preset = "linux-debug" if args.debug else "linux-release"
    subprocess.run(["cmake", "--preset", preset, *options], cwd=ROOT, check=True)
    if args.tools_only:
        targets = ["xbe_recomp", "xbe_symbols", "iso_extract"]
    else:
        targets = ["recompile_SegaGolf" if args.generate_only else "SegaGolf"]
        if args.tests:
            targets += ["SegaGolfNetworkTests", "SegaGolfTouchTests", "SegaGolfSwingTests", "SegaGolfResourceTests"]
    subprocess.run(["cmake", "--build", "--preset", preset, "--target", *targets,
                    "--parallel", str(args.jobs)], cwd=ROOT, check=True)


if __name__ == "__main__":
    main()
