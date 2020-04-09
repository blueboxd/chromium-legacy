// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_PERMISSIONS_UTILS_H_
#define UI_BASE_COCOA_PERMISSIONS_UTILS_H_

#include "ui/base/ui_base_export.h"

namespace ui {

// Heuristic to check screen capture permission.
// Starting on macOS 10.15, the ability to screen capture is restricted and
// requires a permission authorization. There is no direct way to query the
// permission state, so this uses a heuristic to evaluate whether the permission
// has been granted.
UI_BASE_EXPORT bool IsScreenCaptureAllowed();

}  // namespace ui

#endif  // UI_BASE_COCOA_PERMISSIONS_UTILS_H_
