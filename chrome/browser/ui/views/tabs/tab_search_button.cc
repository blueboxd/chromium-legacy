// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_search_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "ui/gfx/paint_vector_icon.h"

namespace {
constexpr int kIconSize = 20;
}

TabSearchButton::TabSearchButton(TabStrip* tab_strip,
                                 views::ButtonListener* listener)
    : NewTabButton(tab_strip, listener) {
  SetImageHorizontalAlignment(HorizontalAlignment::ALIGN_CENTER);
  SetImageVerticalAlignment(VerticalAlignment::ALIGN_MIDDLE);
}

void TabSearchButton::PaintIcon(gfx::Canvas* canvas) {
  // Icon color needs to be updated here as this is called when the hosting
  // window switches between active and incactive states. In each state the
  // foreground color of the tab controls is expected to change.
  SetImage(
      Button::STATE_NORMAL,
      gfx::CreateVectorIcon(kCaretDownIcon, kIconSize, GetForegroundColor()));
  views::ImageButton::PaintButtonContents(canvas);
}
