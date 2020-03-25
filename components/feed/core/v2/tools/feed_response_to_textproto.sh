#!/bin/bash
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Converts a Feed HTTP response from binary to text using proto definitions from
# Chromium.
#
# Usage: feed_response_to_textproto.sh <in.binarypb> <out.textproto>

IN_FILE=$1
OUT_FILE=$2
TMP_FILE=/tmp/trimmedfeedresponse.binarypb

CHROMIUM_SRC=$(realpath $(dirname $(readlink -f $0))/../../../../..)
FEEDPROTO="$CHROMIUM_SRC/components/feed/core/proto"

# Responses start with a 4-byte length value that must be removed.
tail -c +4 $IN_FILE > $TMP_FILE

python3 $CHROMIUM_SRC/components/feed/core/v2/tools/textpb_to_binarypb.py \
  --chromium_path=$CHROMIUM_SRC \
  --output_file=$OUT_FILE \
  --source_file=$TMP_FILE \
  --direction=reverse
