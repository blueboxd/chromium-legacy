#!/usr/bin/env python

# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Adds an sRGB chunk to a png file if one isn't already there.
#
# This is quick and dirty. It doesn't check for iCCP or other chunks that might
# conflict with the sRGB. Obviously, only use this if it's desirable for the
# png file to be treated as sRGB.

import struct
import sys


def make_srgb(path):
    file = open(path, 'r+b')
    magic = file.read(8)
    assert magic == b'\x89\x50\x4e\x47\x0d\x0a\x1a\x0a'

    found_ihdr = False
    after_ihdr = 0
    found_srgb = False
    while True:
        chunk_header = file.read(8)
        (chunk_length, chunk_type) = struct.unpack('>I4s', chunk_header)
        file.seek(chunk_length, 1)
        chunk_footer = file.read(4)
        (chunk_crc,) = struct.unpack('>I', chunk_footer)

        if chunk_type == b'IHDR':
            assert not found_ihdr
            found_ihdr = True
            after_ihdr = file.tell()
        else:
            assert found_ihdr

        if chunk_type == b'sRGB':
            assert not found_srgb
            found_srgb = True

        if chunk_type == b'IEND':
            break

    eof = file.read(1)
    assert len(eof) == 0

    if not found_srgb:
        file.seek(after_ihdr)
        remainder = file.read()
        file.seek(after_ihdr)

        # Length, type, data, CRC
        file.write(b'\x00\x00\x00\x01sRGB\x00\xae\xce\x1c\xe9')

        file.write(remainder)

    file.close()


def main(args):
    for path in args:
        print(path)
        make_srgb(path)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
