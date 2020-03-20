# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import ConfigParser
import glob
import optparse
import os
import shutil
import subprocess
import sys


def embed_version(input_file, output_file, version):
    fin = open(input_file, 'r')
    fout = open(output_file, 'w')
    replace_string = 'UPDATE_VERSION=\"' + version + '\"'

    for line in fin:
        fout.write(line.replace('UPDATE_VERSION=', replace_string))

    fin.close()
    fout.close()

    os.chmod(output_file, 0755)


def parse_options():
    parser = optparse.OptionParser()
    parser.add_option('-i', '--input_file', help='Path to the input script.')
    parser.add_option(
        '-o',
        '--output_file',
        help='Path to where we should output the script')
    parser.add_option(
        '-v',
        '--version',
        help='Version of the application bundle being built.')
    options, _ = parser.parse_args()

    if not options.version:
        parser.error('You must provide a version')

    return options


def main(options):
    embed_version(options.input_file, options.output_file, options.version)
    return 0


if '__main__' == __name__:
    options = parse_options()
    sys.exit(main(options))
