// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_ICON_BUNDLE_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_ICON_BUNDLE_H_

#include "base/macros.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace notifications {

// A wrapper of various format of icon and andriod id.
struct IconBundle {
  IconBundle();
  explicit IconBundle(SkBitmap skbitmap);
  ~IconBundle();

  // The icon bitmap.
  SkBitmap bitmap;

  // TODO(hesen): Handle Android Id.
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_ICON_BUNDLE_H_
