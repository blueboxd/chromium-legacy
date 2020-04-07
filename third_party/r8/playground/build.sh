#!/bin/bash
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Usage:
# Make edits to *.java and *.pgcfg
# Then run: ./build.sh | less
rm -f *.class
javac *.java && \
java -jar ../lib/r8.jar *.class --output . --lib ../../jdk/current --no-minification --pg-conf playground.pgcfg && \
../../android_sdk/public/build-tools/29.0.2/dexdump -d classes.dex > dexdump.txt
