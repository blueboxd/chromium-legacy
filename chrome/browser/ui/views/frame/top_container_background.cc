// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/top_container_background.h"

#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/theme_provider.h"

TopContainerBackground::TopContainerBackground(BrowserView* browser_view)
    : browser_view_(browser_view) {}

void TopContainerBackground::Paint(gfx::Canvas* canvas,
                                   views::View* view) const {
  const ui::ThemeProvider* const theme_provider = view->GetThemeProvider();
  if (theme_provider->HasCustomImage(IDR_THEME_TOOLBAR)) {
    // Calculate the offset of the upper-left corner of the owner view in the
    // browser view. Not all views are directly parented to the browser, so we
    // have to walk up the view hierarchy. This will tell us the offset into the
    // tiled image that will correspond to the upper-left corner of the view.
    int x = 0;
    int y = 0;
    views::View* current = view;
    for (; current != browser_view_; current = current->parent()) {
      x += current->x();
      y += current->y();
    }
    x += browser_view_->frame()->GetThemeBackgroundXInset();

    // Start tiling from coordinates (x, y) in the image into local space,
    // filling the entire local bounds of the owner view.
    canvas->TileImageInt(*theme_provider->GetImageSkiaNamed(IDR_THEME_TOOLBAR),
                         x, y, 0, 0, view->width(), view->height());
  } else {
    canvas->DrawColor(theme_provider->GetColor(ThemeProperties::COLOR_TOOLBAR));
  }
}
