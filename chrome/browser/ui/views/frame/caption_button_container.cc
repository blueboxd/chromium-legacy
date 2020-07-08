// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/caption_button_container.h"

#include "chrome/browser/themes/theme_properties.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/views/widget/widget.h"

void CaptionButtonContainer::OnPaintBackground(gfx::Canvas* canvas) {
  if (paint_frame_background_) {
    const SkColor caption_color = GetThemeProvider()->GetColor(
        GetWidget()->ShouldPaintAsActive()
            ? ThemeProperties::COLOR_FRAME_ACTIVE
            : ThemeProperties::COLOR_FRAME_INACTIVE);
    canvas->DrawColor(caption_color);
  }
  View::OnPaintBackground(canvas);
}
