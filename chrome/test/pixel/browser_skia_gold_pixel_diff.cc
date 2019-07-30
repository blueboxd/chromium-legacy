// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/pixel/browser_skia_gold_pixel_diff.h"

#include "base/logging.h"
#include "chrome/browser/ui/browser_window.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/snapshot/snapshot.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

BrowserSkiaGoldPixelDiff::BrowserSkiaGoldPixelDiff() = default;

BrowserSkiaGoldPixelDiff::~BrowserSkiaGoldPixelDiff() = default;

void BrowserSkiaGoldPixelDiff::Init(views::Widget* widget,
                                    const std::string& screenshot_prefix) {
  SkiaGoldPixelDiff::Init(screenshot_prefix);
  DCHECK(widget);
  widget_ = widget;
}

bool BrowserSkiaGoldPixelDiff::GrabWindowSnapshotInternal(
    gfx::NativeWindow window,
    const gfx::Rect& snapshot_bounds,
    gfx::Image* image) const {
  bool ret = ui::GrabWindowSnapshot(window, snapshot_bounds, image);
  if (!ret) {
    LOG(WARNING) << "Grab snapshot failed";
    return false;
  }
  return true;
}

bool BrowserSkiaGoldPixelDiff::CompareScreenshot(
    const std::string& screenshot_name,
    const views::View* view) const {
  DCHECK(Initialized()) << "Initialize the class before using this method.";
  gfx::Rect rc = view->GetBoundsInScreen();
  gfx::Rect bounds_in_screen = widget_->GetRootView()->GetBoundsInScreen();
  gfx::Rect bounds = widget_->GetRootView()->bounds();
  rc.Offset(bounds.x() - bounds_in_screen.x(),
            bounds.y() - bounds_in_screen.y());
  gfx::Image image;
  bool ret = GrabWindowSnapshotInternal(widget_->GetNativeWindow(), rc, &image);
  if (!ret) {
    return false;
  }
  return SkiaGoldPixelDiff::CompareScreenshot(screenshot_name,
                                              *image.ToSkBitmap());
}
