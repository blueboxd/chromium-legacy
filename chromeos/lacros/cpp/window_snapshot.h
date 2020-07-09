// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LACROS_CPP_WINDOW_SNAPSHOT_H_
#define CHROMEOS_LACROS_CPP_WINDOW_SNAPSHOT_H_

#include <stdint.h>

#include <vector>

#include "base/component_export.h"

namespace lacros {

// bitmap is a 4-byte RGBA bitmap representation of the window. Its size must
// be exactly equal to width * height * 4.
struct COMPONENT_EXPORT(LACROS) WindowSnapshot {
  WindowSnapshot();
  ~WindowSnapshot();
  uint32_t width = 0;
  uint32_t height = 0;
  std::vector<uint8_t> bitmap;
};

}  // namespace lacros

#endif  // CHROMEOS_LACROS_CPP_WINDOW_SNAPSHOT_H_
