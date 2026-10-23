#!/usr/bin/env python3
"""Extract Sega Golf GDX-0010B v3.000 from a local zipped GD-ROM dump.

Uses the publicly documented Golf key from the Citylan GDX-0010C dump notes;
MAME identifies the same PIC for revisions B and C. The FATX signature and
filesystem are checked before accepting the result. Requires PyCryptodome.
"""
import argparse
import json
import mmap
from pathlib import Path, PurePosixPath
import shlex
import zipfile

from disc import exact, both32, records, payload, RAW_SECTOR, DATA_OFFSET
from fatx import decrypt, Fatx

ROOT = Path(__file__).resolve().parents[1]
KEY_SOURCE = "https://www.citylan.it/index.php/Sega_Golf_Club_Network_Pro_Tour_2005_(Rev_C)_(GDX-0010C)"


def prepare(archive):
    output = ROOT / "assets"
    private = ROOT / ".local"
    image = private / "golf-512.fatx"
    encrypted = private / "GOLF_512.BIN"
    if any(path.exists() for path in (output, image, encrypted)):
        raise ValueError("Game preparation outputs already exist; refusing to overwrite")
    private.mkdir(exist_ok=True)
    with zipfile.ZipFile(archive) as disc:
        descriptors = [i for i in disc.infolist() if i.filename.lower().endswith(".gdi")]
        if len(descriptors) != 1 or descriptors[0].file_size > 65536:
            raise ValueError("Expected one small GDI descriptor")
        gdi = disc.read(descriptors[0]).decode("utf-8-sig").splitlines()
        rows = [shlex.split(line) for line in gdi[1:] if line.strip()]
        if len(rows) != int(gdi[0]):
            raise ValueError("GDI track count disagrees with descriptor")
        candidates = [r for r in rows if len(r) == 6 and r[0] == "3" and r[2:4] == ["4", "2352"]]
        if len(candidates) != 1 or candidates[0][5] != "0":
            raise ValueError("Expected a raw MODE1 track 03 without a file offset")
        row = candidates[0]
        start_lba = int(row[1])
        track_name = (PurePosixPath(descriptors[0].filename).parent / row[4]).as_posix()
        track = disc.getinfo(track_name)
        if track.file_size % RAW_SECTOR:
            raise ValueError("Data track contains a partial raw sector")
        with disc.open(track) as stream:
            initial = exact(stream, RAW_SECTOR)
            if initial[:12] != b"\0" + b"\xff" * 10 + b"\0" or initial[15] != 1:
                raise ValueError("Expected raw MODE1 sector framing")
            boot = initial[DATA_OFFSET:DATA_OFFSET + 2048]
            product = boot[64:74].decode("ascii").strip()
            version = boot[74:80].decode("ascii").strip()
            if (product, version) != ("GDX-0010B", "V3.000"):
                raise ValueError(f"This configuration requires GDX-0010B V3.000, found {product} {version}")
            stream.seek(16 * RAW_SECTOR + DATA_OFFSET)
            pvd = exact(stream, 2048)
            if pvd[:7] != b"\x01CD001\x01":
                raise ValueError("Missing ISO9660 primary volume descriptor")
            root = {"lba": both32(pvd, 158), "size": both32(pvd, 166)}
            if root["size"] > 16 * 1024 * 1024:
                raise ValueError("Unreasonable ISO root directory size")
            entries = list(records(b"".join(payload(stream, root, start_lba, track.file_size))))
            selected = [e for e in entries if e["name"] == "GOLF_512.BIN" and not e["directory"]]
            if len(selected) != 1:
                raise ValueError("Missing unique 512 MB Golf container")
            print(f"Extracting {product} {version}: GOLF_512.BIN", flush=True)
            with encrypted.open("xb") as target:
                for block in payload(stream, selected[0], start_lba, track.file_size):
                    target.write(block)
            while stream.read(4 * 1024 * 1024):
                pass
    # Encode the documented big-endian DES key in the existing extractor's
    # PIC-response layout. No PIC firmware image is created or distributed.
    key = bytes.fromhex("D91C02EFC1EF158C")
    pic = bytearray(80)
    pic[0x31:0x38], pic[0x29] = key[:7], key[7]
    encrypted_hash, decrypted_hash = decrypt(encrypted, image, pic)
    with image.open("rb") as source, mmap.mmap(source.fileno(), 0, access=mmap.ACCESS_READ) as data:
        fs = Fatx(data, 512000000)
        files = fs.extract(output)
        report = {"archive": str(archive.resolve()), "product": product, "version": version,
                  "release_date": boot[80:88].decode("ascii"),
                  "data_track": track_name, "data_track_crc32": f"{track.CRC:08x}",
                  "track_zip_crc_verified": True, "key_source": KEY_SOURCE,
                  "entries": entries, "encrypted_sha256": encrypted_hash,
                  "decrypted_sha256": decrypted_hash, "partition_size": 512000000,
                  "cluster_size": fs.cluster_size, "data_offset": fs.data_offset, "files": files}
    (private / "extracted.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"Extracted {len(files)} files, {sum(f['size'] for f in files):,} bytes")
    for entry in files:
        if entry["path"].lower().endswith(".xbe"):
            print(f"XBE: {entry['path']} {entry['size']:,} bytes SHA-256 {entry['sha256']}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    args = parser.parse_args()
    try:
        prepare(args.archive)
    except (OSError, ValueError, KeyError, UnicodeError, ImportError, zipfile.BadZipFile) as error:
        parser.exit(1, f"Preparation stopped: {error}\n")
