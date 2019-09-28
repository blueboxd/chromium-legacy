// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/platform_window_base.h"

namespace ui {

PlatformWindowBase::PlatformWindowBase() = default;

PlatformWindowBase::~PlatformWindowBase() = default;

bool PlatformWindowBase::ShouldWindowContentsBeTransparent() const {
  return false;
}

void PlatformWindowBase::SetZOrderLevel(ZOrderLevel order) {}

ZOrderLevel PlatformWindowBase::GetZOrderLevel() const {
  return ZOrderLevel::kNormal;
}

void PlatformWindowBase::StackAbove(gfx::AcceleratedWidget widget) {}

void PlatformWindowBase::StackAtTop() {}

}  // namespace ui
