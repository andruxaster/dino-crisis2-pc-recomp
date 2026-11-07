"""LZSS used by Dino Crisis 2 PC (Unpack 0x4041F0).
flag byte, bits LSB first: 1 = literal byte, 0 = match of 2 bytes:
  b0, b1 -> offset = b0 | (b1 & 0x0F) << 8 (back from current output),
            length = (b1 >> 4) + 2
"""
def unpack(src, out_len=None):
    out = bytearray(); i = 0; n = len(src)
    while i < n and (out_len is None or len(out) < out_len):
        flags = src[i]; i += 1
        for _ in range(8):
            if i >= n or (out_len is not None and len(out) >= out_len):
                break
            if flags & 1:
                out.append(src[i]); i += 1
            else:
                if i + 1 >= n:
                    i = n; break
                b0, b1 = src[i], src[i + 1]; i += 2
                off = b0 | (b1 & 0x0F) << 8
                ln = (b1 >> 4) + 2
                for _ in range(ln):
                    out.append(out[-off] if off and off <= len(out) else 0)
            flags >>= 1
    return bytes(out)
