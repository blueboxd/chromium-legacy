// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Command-line interface for checking the integrity of .size files.
// Intended to be called from WebAssembly code.

#include <stdlib.h>

#include "tools/binary_size/libsupersize/caspian/file_format.h"
#include "tools/binary_size/libsupersize/caspian/model.h"

caspian::SizeInfo info;

extern "C" {
bool LoadSizeFile(const char* compressed, size_t size) {
  caspian::ParseSizeInfo(compressed, size, &info);
  return true;
}
}
