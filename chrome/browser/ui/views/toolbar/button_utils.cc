// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/button_utils.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"

bool UseWindowsIconsForMinimalUI() {
  return base::win::GetVersion() >= base::win::Version::WIN10;
}
#endif
