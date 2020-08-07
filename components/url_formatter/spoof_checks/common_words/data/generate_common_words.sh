#!/bin/bash
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This script generates common_words.gperf. See README.md for more info.
awk 'length($1) > 2 {print $1}' count_1w.txt \
  | grep -v -f brands_in_common_words_regex.list \
  | head -n 10000 | sort \
  | awk 'BEGIN { print "%%" } { print $0", 0" } END { print "%%" }' \
  > common_words.gperf
