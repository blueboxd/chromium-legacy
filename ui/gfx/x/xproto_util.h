// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_XPROTO_UTIL_H_
#define UI_GFX_X_XPROTO_UTIL_H_

#include <X11/Xlib.h>

#include "base/component_export.h"

namespace x11 {

COMPONENT_EXPORT(X11)
void LogErrorEventDescription(const XErrorEvent& error_event);

}  // namespace x11

#endif  //  UI_GFX_X_XPROTO_UTIL_H_
