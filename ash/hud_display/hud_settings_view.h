// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_HUD_SETTINGS_VIEW_H_
#define ASH_HUD_DISPLAY_HUD_SETTINGS_VIEW_H_

#include "ash/hud_display/hud_constants.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace ash {
namespace hud_display {

class HUDSettingsView : public views::ButtonListener, public views::View {
 public:
  METADATA_HEADER(HUDSettingsView);

  // Use light orange color.
  static constexpr SkColor kDefaultColor =
      SkColorSetARGB(kHUDAlpha, 0xFF, 0xB2, 0x66);

  HUDSettingsView();
  ~HUDSettingsView() override;

  HUDSettingsView(const HUDSettingsView&) = delete;
  HUDSettingsView& operator=(const HUDSettingsView&) = delete;

  // views::ButtonListener
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Shows/hides the view.
  void ToggleVisibility();
};

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_HUD_SETTINGS_VIEW_H_
