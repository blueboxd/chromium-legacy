// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/public/icon_bundle.h"

#include <utility>

namespace notifications {

IconBundle::IconBundle() = default;
IconBundle::IconBundle(SkBitmap skbitmap) : bitmap(std::move(skbitmap)) {}
IconBundle::~IconBundle() = default;

}  // namespace notifications
