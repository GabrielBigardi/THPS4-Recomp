"""THPS4 (PS1) game archive: CD.HED (index) + CD.WAD (data).

CD.HED is a list of (name hash, byte offset in CD.WAD, size) records ending with a zero word.
The name hash is the game's own CRC variant (function at 0x80057D5C in SLUS_014.85): start
with 0xFFFFFFFF; for each character (A-Z lowercased): x = (c ^ crc) & 0xFF, then 8 times
{ crc = rotate_left(crc, 1); if x & 1: crc ^= 0xEDB88320; x >>= 1 }.
"""
import struct


def name_hash(name):
    crc = 0xFFFFFFFF
    for ch in name.encode('latin-1'):
        if 0x41 <= ch <= 0x5A:
            ch += 32
        x = (ch ^ crc) & 0xFF
        for _ in range(8):
            crc = ((crc << 1) | (crc >> 31)) & 0xFFFFFFFF
            if x & 1:
                crc ^= 0xEDB88320
            x >>= 1
    return crc


def read_hed(data):
    ents = []
    for i in range(len(data) // 12):
        h, off, size = struct.unpack_from('<III', data, i * 12)
        if h == 0 and off == 0 and size == 0:
            break
        ents.append((h, off, size))
    return ents


def extract_files(disc, names, outdir):
    """copy the named CD.WAD members out of a disc image (iso.Disc); returns {name: bytes}"""
    import os
    from iso import find_file
    hed = find_file(disc, 'CD.HED')
    wad = find_file(disc, 'CD.WAD')
    if not hed or not wad:
        raise SystemExit('CD.HED / CD.WAD not found - this is not Tony Hawk\'s Pro Skater 4 (PS1)')
    index = {h: (off, size) for h, off, size in read_hed(disc.read_extent(hed['lba'], hed['size']))}
    out = {}
    for n in names:
        if name_hash(n) not in index:
            raise SystemExit('%s not found in CD.WAD' % n)
        off, size = index[name_hash(n)]
        out[n] = disc.read_extent(wad['lba'] + off // 2048, size)
        if outdir:
            os.makedirs(outdir, exist_ok=True)
            with open(os.path.join(outdir, n), 'wb') as f:
                f.write(out[n])
    return out
