#!/bin/sh
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This script is used to launch WebKitTestRunner on ClusterFuzz bots.

BASEDIR=$(dirname "$0")
LIBDIR=$(xcode-select --print-path)/Toolchains/XcodeDefault.xctoolchain/usr/lib/clang/10.0.1/lib/darwin
DYLD_FRAMEWORK_PATH=$BASEDIR DYLD_LIBRARY_PATH=$LIBDIR ./WebKitTestRunner $@
