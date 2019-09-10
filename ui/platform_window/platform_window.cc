// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/platform_window.h"

namespace ui {

PlatformWindow::PlatformWindow() = default;

PlatformWindow::~PlatformWindow() = default;

void PlatformWindow::SetZOrderLevel(ZOrderLevel order) {}

ZOrderLevel PlatformWindow::GetZOrderLevel() const {
  return ZOrderLevel::kNormal;
}

}  // namespace ui
