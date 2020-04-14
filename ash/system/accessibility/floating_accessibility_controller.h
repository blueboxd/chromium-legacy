// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_FLOATING_ACCESSIBILITY_CONTROLLER_H_
#define ASH_SYSTEM_ACCESSIBILITY_FLOATING_ACCESSIBILITY_CONTROLLER_H_

#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/system/accessibility/floating_accessibility_view.h"
#include "ash/system/locale/locale_update_controller_impl.h"

namespace ash {

class FloatingAccessibilityView;

// Controls the floating accessibility menu.
class FloatingAccessibilityController
    : public FloatingAccessibilityView::Delegate,
      public TrayBubbleView::Delegate,
      public LocaleChangeObserver {
 public:
  FloatingAccessibilityController();
  FloatingAccessibilityController(const FloatingAccessibilityController&) =
      delete;
  FloatingAccessibilityController& operator=(
      const FloatingAccessibilityController&) = delete;
  ~FloatingAccessibilityController() override;

  // Starts showing the floating menu when called.
  void Show(FloatingMenuPosition position);
  void SetMenuPosition(FloatingMenuPosition new_position);

 private:
  friend class FloatingAccessibilityControllerTest;
  // FloatingAccessibilityView::Delegate:
  void OnDetailedMenuEnabled(bool enabled) override;
  // TrayBubbleView::Delegate:
  void BubbleViewDestroyed() override;
  // LocaleChangeObserver:
  void OnLocaleChanged() override;

  FloatingAccessibilityView* menu_view_ = nullptr;
  FloatingAccessibilityBubbleView* bubble_view_ = nullptr;
  views::Widget* bubble_widget_ = nullptr;

  bool detailed_view_shown_ = false;

  FloatingMenuPosition position_ = kDefaultFloatingMenuPosition;
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_FLOATING_ACCESSIBILITY_CONTROLLER_H_
