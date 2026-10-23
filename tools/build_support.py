"""Local paths and executable identity shared by the standalone build tools."""
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
IMAGE_SHA256 = "00dd5d68d50104d67e23965c36245a315732d239228e3c8ccf10f5a75b8fab0d"


def verify_image():
    image = ROOT / "assets/golf.xbe"
    with image.open("rb") as stream:
        actual = hashlib.file_digest(stream, "sha256").hexdigest()
    if actual != IMAGE_SHA256:
        raise ValueError("assets/golf.xbe does not match GDX-0010B V3.000")
    return image


def dependency_options(args):
    options = []
    if args.dxc_dir:
        directory = args.dxc_dir.resolve()
        if not (directory / "include/dxc/dxcapi.h").is_file():
            raise ValueError("--dxc-dir must contain include/dxc/dxcapi.h")
        options.append(f"-DFETCHCONTENT_SOURCE_DIR_DXC={directory}")
    if args.deps_dir:
        directory = args.deps_dir.resolve()
        if not directory.is_dir():
            raise ValueError("--deps-dir must be an existing dependency cache")
        for name in ("zydis", "tomlplusplus", "imgui", "sdl3", "vma"):
            source = directory / (name + "-src")
            if source.is_dir():
                options.append(f"-DFETCHCONTENT_SOURCE_DIR_{name.upper()}={source}")
    return options
