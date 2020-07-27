// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_MEDIA_CONTROLS_CHIP_VIEW_H_
#define ASH_SYSTEM_UNIFIED_MEDIA_CONTROLS_CHIP_VIEW_H_

#include "ash/ash_export.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view.h"

namespace views {
class Label;
class ImageView;
}  // namespace views

namespace ash {

// A media controls chip in UnifiedSystemTray bubble. It shows
// information and basic controls of the media currently playing.
class ASH_EXPORT MediaControlsChipView : public views::View {
 public:
  MediaControlsChipView();
  ~MediaControlsChipView() override;
  MediaControlsChipView(MediaControlsChipView&) = delete;
  MediaControlsChipView operator=(MediaControlsChipView&) = delete;

  // views::View:
  void OnPaintBackground(gfx::Canvas* canvas) override;

  // Change the expanded state. 0.0 if collapsed, and 1.0 if expanded.
  // Otherwise, it shows intermediate state. If collapsed, the media controls
  // are hidden.
  void SetExpandedAmount(double expanded_amount);

  const char* GetClassName() const override;

 private:
  views::ImageView* artwork_view_ = nullptr;
  views::View* title_artist_view_ = nullptr;

  views::Label* title_label_ = nullptr;
  views::Label* artist_label_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_MEDIA_CONTROLS_CHIP_VIEW_H_
