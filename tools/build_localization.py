#!/usr/bin/env python3
"""Build English string/sprite metadata from the owner's verified game files."""
import argparse
import gzip
import json
from pathlib import Path
import re
import struct
from build_support import ROOT, verify_image


def fnv(data):
    value = 14695981039346656037
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return value


def texture_signature(data):
    if len(data) < 32:
        raise ValueError("Texture too small")
    sample = bytearray()
    for index in range(16):
        offset = (len(data) - 32) * index // 15
        sample.extend(data[offset:offset + 32])
    return fnv(sample)


def sections(image):
    base = struct.unpack_from("<I", image, 0x104)[0]
    count, headers = struct.unpack_from("<II", image, 0x11C)
    if not 0 < count < 100:
        raise ValueError("Invalid XBE section count")
    return [struct.unpack_from("<6I", image, headers - base + index * 56)[1:5]
            for index in range(count)]


def string_at(image, address):
    for virtual, _, raw, size in sections(image):
        if virtual <= address < virtual + size:
            offset = raw + address - virtual
            end = image.index(b"\0", offset, min(raw + size, offset + 4096))
            return image[offset:end]
    raise ValueError(f"String address {address:08X} is not in an image section")


def formats(text):
    return [value for value in re.findall(r"%(?:[-+ #0]*\d*(?:\.\d+)?(?:hh|ll|[hljztL])?)([a-zA-Z%])", text)
            if value != "%"]


def sprite_package(path):
    data = path.read_bytes()
    if data[:4] == b"SGGZ":
        expected = struct.unpack_from("<I", data, 4)[0]
        data = gzip.decompress(data[16:])
        if len(data) != expected:
            raise ValueError(f"Wrong decompressed size: {path}")
    xpr, textures = struct.unpack_from("<II", data, 4)
    count, records = struct.unpack_from("<II", data, 24)
    if data[xpr:xpr + 4] != b"XPR0" or records + count * 32 > xpr:
        raise ValueError(f"Invalid XST layout: {path}")
    header_size = struct.unpack_from("<I", data, xpr + 8)[0]
    pages = []
    for index in range(textures):
        _, offset, _, fmt, _ = struct.unpack_from("<5I", data, xpr + 12 + index * 20)
        width, height = 1 << ((fmt >> 20) & 15), 1 << ((fmt >> 24) & 15)
        if (fmt >> 8) & 255 != 15:
            raise ValueError(f"Unsupported sprite format: {path}")
        size = width * height  # DXT5: one byte per texel
        pixels = data[xpr + header_size + offset:xpr + header_size + offset + size]
        if len(pixels) != size:
            raise ValueError(f"Truncated sprite texture: {path}")
        pages.append((width, height, texture_signature(pixels)))
    sprites = [struct.unpack_from("<II4f4H", data, records + index * 32) for index in range(count)]
    return pages, sprites


def build(output):
    image = verify_image().read_bytes()
    catalog = json.loads((ROOT / "localization/en.json").read_text())
    lines = ["// Generated from the owner's verified game. Do not check in.", "#pragma once",
             '#include "game/language.h"', "namespace golf::language {", "inline constexpr StringPatch kStrings[] = {"]
    for value, english in catalog["strings"].items():
        address = int(value, 0)
        original = string_at(image, address)
        encoded = english.encode("ascii")
        if len(encoded) > len(original):
            raise ValueError(f"English string too long at {value}: {len(encoded)} > {len(original)}")
        if formats(original.decode("cp932")) != formats(english):
            raise ValueError(f"Format arguments differ at {value}")
        lines.append(f"    {{0x{address:08X}, {len(original)}, 0x{fnv(original):016X}ULL, {json.dumps(english)}}},")
    lines += ["};", "inline constexpr SpritePatch kSprites[] = {"]
    total = 0
    for filename, labels in catalog["sprites"].items():
        pages, sprites = sprite_package(ROOT / "assets/media/sprite" / filename)
        for value, translation in labels.items():
            english = translation if isinstance(translation, str) else translation["text"]
            box = [0.0, 0.0, 1.0, 1.0] if isinstance(translation, str) else translation["box"]
            if len(box) != 4 or not (0 <= box[0] < box[2] <= 1 and 0 <= box[1] < box[3] <= 1):
                raise ValueError(f"Invalid label bounds: {filename}:{value}")
            english.encode("ascii")
            texture, _, u0, v0, u1, v1, *_ = sprites[int(value)]
            width, height, signature = pages[texture]
            # XST stores top-down sprite rectangles; the original sprite
            # renderer samples the XPR's bottom-up texture with reversed V.
            v0, v1 = 1 - v1, 1 - v0
            coords = ", ".join(repr(value) + "f" for value in (u0, v0, u1, v1))
            bounds = ", ".join(repr(float(value)) + "f" for value in box)
            lines.append(f"    {{0x{signature:016X}ULL, {width}, {height}, {coords}, {json.dumps(english)}, {bounds}}},")
            total += 1
    lines += ["};", "}  // namespace golf::language", ""]
    text = "\n".join(lines)
    output.parent.mkdir(parents=True, exist_ok=True)
    if not output.exists() or output.read_text() != text:
        output.write_text(text)
    print(f"English metadata: {len(catalog['strings'])} messages, {total} sprite labels")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "generated/game/language_tables.h")
    args = parser.parse_args()
    build(args.output)
