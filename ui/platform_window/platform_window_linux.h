// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_PLATFORM_WINDOW_LINUX_H_
#define UI_PLATFORM_WINDOW_PLATFORM_WINDOW_LINUX_H_

#include "ui/platform_window/platform_window_base.h"

namespace ui {

// Linux extensions to the PlatformWindowBase.
class PlatformWindowLinux : public PlatformWindowBase {
 public:
  PlatformWindowLinux();
  ~PlatformWindowLinux() override;

  // X11-specific.  Returns whether an XSync extension is available at the
  // current platform.
  virtual bool IsSyncExtensionAvailable() const;
  // X11-specific.  Handles CompleteSwapAfterResize event coming from the
  // compositor observer.
  virtual void OnCompleteSwapAfterResize();
};

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_PLATFORM_WINDOW_LINUX_H_
