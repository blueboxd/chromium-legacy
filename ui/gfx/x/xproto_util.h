// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_XPROTO_UTIL_H_
#define UI_GFX_X_XPROTO_UTIL_H_

#include <cstdint>

#include "base/component_export.h"

namespace x11 {

COMPONENT_EXPORT(X11)
void LogErrorEventDescription(unsigned long serial,
                              uint8_t error_code,
                              uint8_t request_code,
                              uint8_t minor_code);

}  // namespace x11

#endif  //  UI_GFX_X_XPROTO_UTIL_H_
