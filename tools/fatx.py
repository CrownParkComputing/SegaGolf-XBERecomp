"""FATX reader and Chihiro DES conversion, shared with the local Chihiro ports."""
import array
import hashlib
import struct


def text(data):
    value = data.split(b"\0", 1)[0].decode("ascii")
    if not value or value in (".", "..") or any(c in value for c in "/\\:"):
        raise ValueError("Unsafe or empty filename")
    return value

def swap_blocks(data):
    blocks = array.array("Q")
    if blocks.itemsize != 8:
        raise ValueError("This host has no 64-bit array element")
    blocks.frombytes(data)
    blocks.byteswap()
    return blocks.tobytes()

def decrypt(source, destination, pic):
    from Crypto.Cipher import DES

    key = (pic[0x31:0x38] + pic[0x29:0x2a])[::-1]
    cipher = DES.new(key, DES.MODE_ECB)
    if source.stat().st_size % 8:
        raise ValueError("Encrypted container is not DES block aligned")
    encrypted_hash = hashlib.sha256()
    decrypted_hash = hashlib.sha256()
    destination.parent.mkdir(parents=True, exist_ok=True)
    first = True
    with source.open("rb") as reader, destination.open("xb") as writer:
        while block := reader.read(4 * 1024 * 1024):
            plain = swap_blocks(cipher.decrypt(swap_blocks(block)))
            if first and plain[:4] != b"FATX":
                raise ValueError("Decryption did not produce a FATX superblock")
            first = False
            encrypted_hash.update(block)
            decrypted_hash.update(plain)
            writer.write(plain)
    if first:
        raise ValueError("Empty game container")
    return encrypted_hash.hexdigest(), decrypted_hash.hexdigest()

class Fatx:
    def __init__(self, data, partition_size):
        self.data = data
        if data[:4] != b"FATX":
            raise ValueError("Missing FATX superblock")
        _, sectors, self.root = struct.unpack_from("<III", data, 4)
        if sectors == 0 or sectors & (sectors - 1) or sectors > 128:
            raise ValueError("Invalid FATX cluster size")
        self.cluster_size = sectors * 512
        self.cluster_count = partition_size // self.cluster_size + 1
        self.entry_size = 2 if self.cluster_count < 0xfff0 else 4
        self.end = 0xfff8 if self.entry_size == 2 else 0xfffffff8
        fat_size = self.cluster_count * self.entry_size
        self.data_offset = 4096 + ((fat_size + 4095) & ~4095)
        self.directories = set()
        if self.data_offset >= len(data):
            raise ValueError("FATX data area lies outside the container")

    def chain(self, cluster):
        seen = set()
        while cluster < self.end:
            if cluster < 1 or cluster >= self.cluster_count or cluster in seen:
                raise ValueError("Invalid or cyclic FATX cluster chain")
            seen.add(cluster)
            start = self.data_offset + (cluster - 1) * self.cluster_size
            if start + self.cluster_size > len(self.data):
                raise ValueError("FATX cluster lies outside the decrypted container")
            yield self.data[start:start + self.cluster_size]
            offset = 4096 + cluster * self.entry_size
            cluster = int.from_bytes(self.data[offset:offset + self.entry_size], "little")

    def extract(self, destination):
        files = []

        def directory(cluster, path, depth=0):
            if cluster in self.directories or depth > 64:
                raise ValueError("Cyclic or excessively nested FATX directory")
            self.directories.add(cluster)
            path.mkdir()
            names = set()
            for block in self.chain(cluster):
                for offset in range(0, len(block), 64):
                    entry = block[offset:offset + 64]
                    size, attributes = entry[0], entry[1]
                    if size in (0, 255):
                        return
                    if size == 0xe5:
                        continue
                    if size > 42:
                        raise ValueError("Invalid FATX filename length")
                    name = text(entry[2:2 + size])
                    if name.casefold() in names:
                        raise ValueError("Duplicate FATX filename")
                    names.add(name.casefold())
                    if attributes & 8:
                        continue
                    first, length = struct.unpack_from("<II", entry, 44)
                    target = path / name
                    if attributes & 16:
                        directory(first, target, depth + 1)
                        continue
                    digest = hashlib.sha256()
                    remaining = length
                    with target.open("xb") as output:
                        if remaining:
                            for contents in self.chain(first):
                                chunk = contents[:remaining]
                                output.write(chunk)
                                digest.update(chunk)
                                remaining -= len(chunk)
                                if not remaining:
                                    break
                    if remaining:
                        raise ValueError("FATX file chain ended before its declared length")
                    files.append({"path": target.relative_to(destination).as_posix(),
                                  "size": length, "sha256": digest.hexdigest()})

        directory(self.root, destination)
        return files
