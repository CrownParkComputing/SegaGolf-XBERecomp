"""Read bounded ISO9660 extents from raw Chihiro MODE1 GD-ROM sectors."""
RAW_SECTOR = 2352
DATA_OFFSET = 16
DATA_SECTOR = 2048


def exact(stream, count):
    data = stream.read(count)
    if len(data) != count:
        raise ValueError("Truncated disc data")
    return data

def bcd(value):
    if value & 15 > 9 or value >> 4 > 9:
        raise ValueError("Invalid raw-sector MSF address")
    return (value >> 4) * 10 + (value & 15)

def both32(data, offset):
    little = int.from_bytes(data[offset:offset + 4], "little")
    big = int.from_bytes(data[offset + 4:offset + 8], "big")
    if little != big:
        raise ValueError("ISO9660 byte-order copies disagree")
    return little

def records(data):
    position = 0
    while position < len(data):
        length = data[position]
        if length == 0:
            position = (position // DATA_SECTOR + 1) * DATA_SECTOR
            continue
        if length < 34 or position + length > len(data):
            raise ValueError("Invalid ISO9660 directory record")
        record = data[position:position + length]
        position += length
        name_length = record[32]
        if 33 + name_length > len(record):
            raise ValueError("Truncated ISO9660 filename")
        raw_name = record[33:33 + name_length]
        if raw_name in (b"\0", b"\1"):
            continue
        name = raw_name.decode("ascii").split(";", 1)[0]
        if not name or name in (".", "..") or any(c in name for c in "/\\:"):
            raise ValueError("Unsafe ISO9660 filename")
        if record[25] & 0x80:
            raise ValueError("Multi-extent ISO9660 files are unsupported")
        yield {"name": name, "lba": both32(record, 2),
               "size": both32(record, 10), "directory": bool(record[25] & 2)}

def payload(stream, entry, start_lba, track_size):
    sector = entry["lba"] - start_lba
    count = (entry["size"] + DATA_SECTOR - 1) // DATA_SECTOR
    if sector < 0 or (sector + count) * RAW_SECTOR > track_size:
        raise ValueError("ISO9660 extent lies outside the data track")
    stream.seek(sector * RAW_SECTOR)
    remaining = entry["size"]
    while count:
        batch = min(count, 1024)
        raw = exact(stream, batch * RAW_SECTOR)
        data = b"".join(raw[i + DATA_OFFSET:i + DATA_OFFSET + DATA_SECTOR]
                        for i in range(0, len(raw), RAW_SECTOR))
        yield data[:remaining]
        remaining -= min(len(data), remaining)
        count -= batch
