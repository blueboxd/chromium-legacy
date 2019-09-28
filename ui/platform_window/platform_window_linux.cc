// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/platform_window_linux.h"

namespace ui {

PlatformWindowLinux::PlatformWindowLinux() = default;

PlatformWindowLinux::~PlatformWindowLinux() = default;

bool PlatformWindowLinux::IsSyncExtensionAvailable() const {
  return false;
}

void PlatformWindowLinux::OnCompleteSwapAfterResize() {}

}  // namespace ui
