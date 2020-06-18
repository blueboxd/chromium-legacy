// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_HUD_DISPLAY_H_
#define ASH_HUD_DISPLAY_HUD_DISPLAY_H_

#include "base/sequence_checker.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace hud_display {

// HUDDisplayView class can be used to display a system monitoring overview.
class HUDDisplayView : public views::WidgetDelegateView {
 public:
  // Default HUDDisplayView height.
  static constexpr size_t kDefaultHUDHeight = 300;

  // Border width inside the HUDDisplayView rectangle around contents.
  static constexpr size_t kHUDInset = 5;

  HUDDisplayView();
  ~HUDDisplayView() override;

  HUDDisplayView(const HUDDisplayView&) = delete;
  HUDDisplayView& operator=(const HUDDisplayView&) = delete;

  // WidgetDelegate:
  views::ClientView* CreateClientView(views::Widget* widget) override;
  void OnWidgetInitialized() override;

  // Destroys global instance.
  static void Destroy();

  // Creates/Destroys global singleton.
  static void Toggle();

 private:
  SEQUENCE_CHECKER(ui_sequence_checker_);
};

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_HUD_DISPLAY_H_
