// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard.h"

#if defined(USE_OZONE)
#include "ui/base/clipboard/clipboard_ozone.h"
#include "ui/base/ui_base_features.h"
#endif

#if defined(USE_X11)
#include "ui/base/clipboard/clipboard_x11.h"
#endif

namespace ui {

// Clipboard factory method.
Clipboard* Clipboard::Create() {
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform())
    return new ClipboardOzone();
#endif

#if defined(USE_X11)
  return new ClipboardX11();
#else
  NOTREACHED();
  return nullptr;
#endif
}

}  // namespace ui
