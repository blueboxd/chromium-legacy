// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/platform_screen.h"

namespace ui {

PlatformScreen::PlatformScreen() = default;
PlatformScreen::~PlatformScreen() = default;

std::string PlatformScreen::GetCurrentWorkspace() {
  NOTIMPLEMENTED_LOG_ONCE();
  return {};
}

}  // namespace ui
