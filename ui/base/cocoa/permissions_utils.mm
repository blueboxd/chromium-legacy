// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/permissions_utils.h"

#include <CoreGraphics/CoreGraphics.h>
#include <Foundation/Foundation.h>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"

namespace ui {

// Screen Capture is considered allowed if the name of at least one normal
// or dock window running on another process is visible.
// See https://crbug.com/993692.
bool IsScreenCaptureAllowed() {
  if (@available(macOS 10.15, *)) {
    base::ScopedCFTypeRef<CFArrayRef> window_list(
        CGWindowListCopyWindowInfo(kCGWindowListOptionAll, kCGNullWindowID));
    int current_pid = [[NSProcessInfo processInfo] processIdentifier];
    for (NSDictionary* window in base::mac::CFToNSCast(window_list.get())) {
      NSNumber* window_pid =
          [window objectForKey:base::mac::CFToNSCast(kCGWindowOwnerPID)];
      if (!window_pid || [window_pid integerValue] == current_pid)
        continue;

      NSString* window_name =
          [window objectForKey:base::mac::CFToNSCast(kCGWindowName)];
      if (!window_name)
        continue;

      NSNumber* layer =
          [window objectForKey:base::mac::CFToNSCast(kCGWindowLayer)];
      if (!layer)
        continue;

      NSInteger layer_integer = [layer integerValue];
      if (layer_integer == CGWindowLevelForKey(kCGNormalWindowLevelKey) ||
          layer_integer == CGWindowLevelForKey(kCGDockWindowLevelKey)) {
        return true;
      }
    }
    return false;
  }

  // Screen capture is always allowed in older macOS versions.
  return true;
}

}  // namespace ui
