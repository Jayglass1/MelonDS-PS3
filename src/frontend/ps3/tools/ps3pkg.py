#!/usr/bin/env python3
"""PARAM.SFO and NPDRM .pkg builder (Python 3).

A port of the Python 2 tools that ship with ps3dev (sfo.py and pkg.py), kept
step-for-step so the output follows the same format, with two changes:

* pkgcrypt.so (a Python 2 C extension) is replaced by the pure-Python version
  that pkg.py itself carries as its reference implementation: a keystream made
  of SHA-1(64-byte context) XORed over the data, with a big-endian 64-bit
  counter at context[0x38:0x40] incremented per 16-byte block.
* directory listings are sorted, so the package contents are reproducible.

Usage:
    ps3pkg.py sfo  --xml sfo.xml [--title T] [--appid ID] out.SFO
    ps3pkg.py pkg  --contentid CID  dir  out.pkg
    ps3pkg.py list file.pkg           # decrypts and lists the contents
    ps3pkg.py check file.pkg dir      # decrypts and compares against dir

"pkg" makes the unfinalized package; run package_finalize on a copy of it to
get one that installs on custom firmware.
"""
import hashlib
import os
import struct
import sys
import xml.dom.minidom

TYPE_NPDRMSELF = 0x1
TYPE_RAW = 0x3
TYPE_DIRECTORY = 0x4
TYPE_OVERWRITE_ALLOWED = 0x80000000


def sha1(data):
    return hashlib.sha1(data).digest()


# ---- PARAM.SFO --------------------------------------------------------------

SFO_MAGIC = 0x46535000
SFO_STRING = 2
SFO_INT = 4


def align(num, alignment):
    return (num + alignment - 1) & ~(alignment - 1)


def _text(node):
    return ''.join(c.data for c in node.childNodes if c.nodeType == c.TEXT_NODE).strip()


def convert_to_sfo(xml_path, force_title=None, force_appid=None):
    dom = xml.dom.minidom.parse(xml_path)
    kvs = []
    for node in dom.getElementsByTagName("value"):
        if not node.hasAttributes():
            continue
        vtype = node.getAttribute("type") or None
        name = node.getAttribute("name") or None
        if name is None or vtype is None:
            continue
        if name == "TITLE" and force_title is not None:
            kvs.append((name, force_title))
        elif name == "TITLE_ID" and force_appid is not None:
            kvs.append((name, force_appid))
        elif vtype == "string":
            kvs.append((name, _text(node)))
        elif vtype == "integer":
            kvs.append((name, int(_text(node))))

    def alignment_for(key):
        return {"TITLE": 0x80, "LICENSE": 0x200, "TITLE_ID": 0x10}.get(key, 4)

    entries = []
    keyoff = 0
    valueoff = 0
    for k, v in kvs:
        if isinstance(v, int):
            vtype, vlen, padded = SFO_INT, 4, 4
        else:
            vlen = len(v.encode("utf-8")) + 1
            vtype, padded = SFO_STRING, align(vlen, alignment_for(k))
        # key_off u16, unk1 u8 (=4), value_type u8, value_len, padded_len, value_off
        entries.append(struct.pack("<HBBIII", keyoff, 4, vtype, vlen, padded, valueoff))
        keyoff += len(k) + 1
        valueoff += padded

    header_len = 20
    key_offset = header_len + 0x10 * len(kvs)
    value_offset = align(key_offset + keyoff, 4)
    pad = value_offset - (key_offset + keyoff)

    out = struct.pack("<IIIII", SFO_MAGIC, 0x00000101, key_offset, value_offset, len(kvs))
    out += b"".join(entries)
    for k, _ in kvs:
        out += k.encode("ascii") + b"\0"
    out += b"\0" * pad
    for k, v in kvs:
        if isinstance(v, int):
            out += struct.pack("<I", v)
        else:
            raw = v.encode("utf-8")
            out += raw + b"\0" + b"\0" * (align(len(raw) + 1, alignment_for(k)) - (len(raw) + 1))
    return out


def list_sfo(data):
    magic, unk1, key_off, val_off, count = struct.unpack("<IIIII", data[:20])
    assert magic == SFO_MAGIC and unk1 == 0x101
    res = {}
    for i in range(count):
        k_off, _u, vtype, vlen, _pad, v_off = struct.unpack("<HBBIII", data[20 + 16 * i:36 + 16 * i])
        key = data[key_off + k_off:].split(b"\0", 1)[0].decode()
        raw = data[val_off + v_off:]
        res[key] = (struct.unpack("<I", raw[:4])[0] if vtype == SFO_INT
                    else raw.split(b"\0", 1)[0].decode())
    return res


# ---- pkgcrypt (pure Python reference implementation) ---------------------------

def key_to_context(key16):
    key = bytes(key16[:16])
    return bytearray(key[0:8] + key[0:8] + key[8:16] + key[8:16] + b"\0" * 0x20)


def set_context_num(ctx, num):
    ctx[0x38:0x40] = struct.pack(">Q", num)


def crypt(ctx, data, length):
    """XOR `length` bytes of data with the keystream; advances ctx."""
    out = bytearray()
    offset = 0
    counter = struct.unpack(">Q", bytes(ctx[0x38:0x40]))[0]
    while length > 0:
        n = min(length, 0x10)
        stream = sha1(bytes(ctx[:0x40]))
        chunk = data[offset:offset + n]
        out += bytes(a ^ b for a, b in zip(chunk, stream))
        offset += n
        counter = (counter + 1) & 0xFFFFFFFFFFFFFFFF
        ctx[0x38:0x40] = struct.pack(">Q", counter)
        length -= n
    return bytes(out)


# ---- .pkg --------------------------------------------------------------------

class FileEntry:
    def __init__(self, name, size, flags):
        self.name = name
        self.name_off = 0
        self.name_len = len(name.encode())
        self.off = 0
        self.size = size
        self.flags = flags

    def pack(self):
        return struct.pack(">IIQQII", self.name_off, self.name_len, self.off, self.size, self.flags, 0)


def pack_header(magic, ptype, pkg_info_off, unk1, head_size, item_count, package_size,
                data_off, data_size, content_id, qa_digest, klicensee):
    return struct.pack(">IIIIIIQQQ", magic, ptype, pkg_info_off, unk1, head_size, item_count,
                       package_size, data_off, data_size) + content_id + qa_digest + klicensee


def get_files(files, folder, original):
    entries = sorted(os.listdir(folder))
    paths = [os.path.join(folder, e) for e in entries]
    ordered = [p for p in paths if not os.path.isdir(p)] + [p for p in paths if os.path.isdir(p)]
    for path in ordered:
        newpath = path.replace("\\", "/")[len(original):].lstrip("/")
        if os.path.isdir(path):
            files.append(FileEntry(newpath, 0, TYPE_OVERWRITE_ALLOWED | TYPE_DIRECTORY))
            get_files(files, path, original)
        else:
            size = os.path.getsize(path)
            flags = TYPE_OVERWRITE_ALLOWED | TYPE_RAW
            if newpath == "USRDIR/EBOOT.BIN":
                size = ((size - 0x30 + 63) & ~63) + 0x30
                flags = TYPE_OVERWRITE_ALLOWED | TYPE_NPDRMSELF
            files.append(FileEntry(newpath, size, flags))


def pack(folder, contentid, outname):
    folder = folder.rstrip("/\\") + "/"
    cid = contentid.encode()
    cid48 = cid[:0x30].ljust(0x30, b"\0")
    drm_type = 3  # free

    meta_block = lambda data_size: struct.pack(
        ">IIIIIIIIIIIHHIIIHH",
        1, 4, drm_type, 2,              # unk1, unk2, drmType, unk4
        4, 5, 3, 4,                     # unk21..unk24 (5 = gameexec)
        0xE, 4, 8, 0, 0,                # unk31..unk33, secondaryVersion, unk34 (0xE = normal)
        data_size, 5, 4, 0x1061, 0)     # dataSize, unk42, unk43, packagedBy, packageVersion

    files = []
    get_files(files, folder, folder)
    item_count = len(files)

    data = b""
    file_off = 0x20 * len(files)
    for f in files:
        f.name_off = file_off
        file_off += (f.name_len + 0x0F) & ~0x0F
    for f in files:
        f.off = file_off
        file_off += (f.size + 0x0F) & ~0x0F
        data += f.pack()
    for f in files:
        aligned = (f.name_len + 0x0F) & ~0x0F
        data += f.name.encode() + b"\0" * (aligned - f.name_len)
    file_desc_len = len(data)

    qadigest = hashlib.sha1()
    for f in files:
        if f.flags & 0xFF == TYPE_DIRECTORY:
            continue
        with open(os.path.join(folder, f.name), "rb") as fp:
            file_data = fp.read()
        qadigest.update(file_data)
        file_sha1 = sha1(file_data)

        if file_data[0:9] == b"SCE\0\0\0\0\x02\x80":
            # SelfHeader: magic, headerVer, flags, type, meta, headerSize, encryptedSize,
            # unknown, AppInfo, elf, phdr, shdr, phdrOffsets, sceversion, digest, digestSize
            sh = struct.unpack(">IIHHIQQQQQQQQQQQ", file_data[:104])
            app_info_off, digest_off = sh[8], sh[14]
            app_type = struct.unpack(">QIIQ", file_data[app_info_off:app_info_off + 24])[2]
            found = False
            while not found:
                d_type, d_size, d_next = struct.unpack(">IIQ", file_data[digest_off:digest_off + 16])
                if d_type == 3:
                    found = True
                else:
                    digest_off += d_size
                if d_next != 1:
                    break
            digest_off += 16
            if app_type == 8 and found:
                data += file_data[0:digest_off]
                meta = struct.pack(">IIII", 0x4E504400, 1, drm_type, 1) + cid48 + file_sha1[:16]
                not_sha1 = bytes((~b) & 0xFF for b in file_sha1[:16])
                xor = bytearray()
                for i in range(16):
                    xor.append(((1 if i == 0xF else 0) ^ not_sha1[i] ^ 0xAA) & 0xFF)
                meta += not_sha1 + bytes(xor) + b"\0" * 0x10
                data += meta
                data += file_data[digest_off + 0x80:]
            else:
                data += file_data
        else:
            data += file_data
        data += b"\0" * (((f.size + 0x0F) & ~0x0F) - len(file_data))

    data_size = len(data)
    header0 = pack_header(0x7F504B47, 1, 0xC0, 5, 0x80, item_count, data_size + 0x1A0,
                          0x140, data_size, b"\0" * 0x30, b"\0" * 0x10, b"\0" * 0x10)
    qadigest.update(header0)
    qadigest.update(data[:file_desc_len])
    qa = qadigest.digest()[:16]

    ctx = key_to_context(qa)
    set_context_num(ctx, 0xFFFFFFFFFFFFFFFF)
    licensee = crypt(ctx, b"\0" * 0x10, 0x10)

    header = pack_header(0x7F504B47, 1, 0xC0, 5, 0x80, item_count, data_size + 0x1A0,
                         0x140, data_size, cid48, qa, licensee)
    out = header
    header_sha = sha1(header)[3:19]
    out += header_sha

    meta_data = meta_block(data_size)
    meta_sha = sha1(meta_data)[3:19]
    pad_enc = crypt(key_to_context(meta_sha), b"\0" * 0x30, 0x30)
    pad_enc2 = crypt(key_to_context(header_sha), pad_enc, 0x30)
    out += pad_enc2 + meta_data + meta_sha + pad_enc
    out += crypt(key_to_context(qa), data, data_size)
    out += b"\0" * 0x60

    with open(outname, "wb") as fp:
        fp.write(out)
    return item_count, data_size


def read_pkg(path):
    """Decrypts a package. Returns (header fields, [(entry, bytes)])."""
    with open(path, "rb") as fp:
        raw = fp.read()
    (magic, ptype, info_off, _u1, head_size, item_count, package_size,
     data_off, data_size) = struct.unpack(">IIIIIIQQQ", raw[:48])
    content_id = raw[48:96]
    qa = raw[96:112]
    assert magic == 0x7F504B47 and ptype == 1, "not a debug-type PKG"
    enc = raw[data_off:data_off + data_size]
    dec = crypt(key_to_context(qa), enc, data_size)
    out = []
    for i in range(item_count):
        n_off, n_len, f_off, f_size, flags, pad = struct.unpack(">IIQQII", dec[0x20 * i:0x20 * i + 0x20])
        name = dec[n_off:n_off + n_len].split(b"\0", 1)[0].decode()
        out.append((dict(name=name, off=f_off, size=f_size, flags=flags), dec[f_off:f_off + f_size]))
    info = dict(item_count=item_count, package_size=package_size, data_off=data_off,
                data_size=data_size, content_id=content_id.split(b"\0", 1)[0].decode(),
                file_size=len(raw), qa=qa.hex())
    return info, out


def main(argv):
    cmd = argv[1] if len(argv) > 1 else ""
    if cmd == "sfo":
        xml_path = title = appid = out = None
        i = 2
        while i < len(argv):
            if argv[i] == "--xml": xml_path = argv[i + 1]; i += 2
            elif argv[i] == "--title": title = argv[i + 1]; i += 2
            elif argv[i] == "--appid": appid = argv[i + 1]; i += 2
            else: out = argv[i]; i += 1
        if not (xml_path and out):
            print(__doc__); return 2
        with open(out, "wb") as fp:
            fp.write(convert_to_sfo(xml_path, title, appid))
        return 0
    if cmd == "pkg" and "--contentid" in argv and len(argv) == 6:
        cid = argv[argv.index("--contentid") + 1]
        rest = [a for a in argv[2:] if a not in ("--contentid", cid)]
        n, size = pack(rest[0], cid, rest[1])
        print("packed %d entries, %d bytes of data" % (n, size))
        return 0
    if cmd == "list" and len(argv) == 3:
        info, files = read_pkg(argv[2])
        print("content id: %s, %d entries, package %d bytes" % (info["content_id"], info["item_count"], info["file_size"]))
        for e, _ in files:
            kind = {TYPE_NPDRMSELF: "NPDRM SELF", TYPE_DIRECTORY: "directory", TYPE_RAW: "raw data"}.get(e["flags"] & 0xFF, "unknown")
            print("  %-10s %11d  %s" % (kind, e["size"], e["name"]))
        return 0
    if cmd == "check" and len(argv) == 4:
        info, files = read_pkg(argv[2])
        bad = 0
        for e, blob in files:
            if e["flags"] & 0xFF == TYPE_DIRECTORY:
                continue
            with open(os.path.join(argv[3], e["name"]), "rb") as fp:
                orig = fp.read()
            same = blob[:len(orig)] == orig and not any(blob[len(orig):])
            if e["flags"] & 0xFF == TYPE_NPDRMSELF:
                # The packager rewrites one 0x80-byte NPDRM meta block, so the
                # EBOOT may differ from the original only inside that window,
                # and anything after it must be zero padding.
                diffs = [i for i in range(len(orig)) if blob[i] != orig[i]]
                same = (len(blob) >= len(orig) and not any(blob[len(orig):])
                        and (not diffs or diffs[-1] - diffs[0] < 0x80))
            print("  %-22s %s" % (e["name"], "ok" if same else "MISMATCH"))
            bad += 0 if same else 1
        return 1 if bad else 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
