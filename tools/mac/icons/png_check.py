#!/usr/bin/env python

# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Does a quick parse of a png file, to make sure that no unneeded png chunks are
# present in the file.

import struct
import sys


def verify(path):
    file = open(path, 'rb')
    magic = file.read(8)
    assert magic == b'\x89\x50\x4e\x47\x0d\x0a\x1a\x0a'

    chunk_types = []
    while True:
        chunk_header = file.read(8)
        (chunk_length, chunk_type) = struct.unpack('>I4s', chunk_header)
        file.seek(chunk_length, 1)
        chunk_footer = file.read(4)
        (chunk_crc,) = struct.unpack('>I', chunk_footer)

        chunk_types.append(chunk_type)

        if chunk_type == b'IEND':
            break

    assert chunk_types == [b'IHDR', b'sRGB', b'IDAT', b'IEND']
    # Some images showed up with [b'IHDR', b'sRGB', b'PLTE', b'tRNS', b'IDAT',
    # b'IEND'] but these wound up being smaller when compressed with
    # optipng/advpng without being in indexed-color mode.

    eof = file.read(1)
    assert len(eof) == 0

    file.close()


def main(args):
    for path in args:
        print(path)
        verify(path)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
