// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_SCREEN_PRIVACY_SCREEN_TOAST_VIEW_H_
#define ASH_SYSTEM_PRIVACY_SCREEN_PRIVACY_SCREEN_TOAST_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace ash {

class FeaturePodIconButton;
class PrivacyScreenToastLabelView;

// The view shown inside the privacy screen toast bubble.
class ASH_EXPORT PrivacyScreenToastView : public views::View {
 public:
  explicit PrivacyScreenToastView(views::ButtonListener* button_listener);
  ~PrivacyScreenToastView() override;
  PrivacyScreenToastView(PrivacyScreenToastView&) = delete;
  PrivacyScreenToastView operator=(PrivacyScreenToastView&) = delete;

  // Updates the toast with whether the privacy screen is enabled.
  void SetPrivacyScreenEnabled(bool enabled);

 private:
  // views::View:
  gfx::Size CalculatePreferredSize() const override;

  FeaturePodIconButton* button_ = nullptr;
  PrivacyScreenToastLabelView* label_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PRIVACY_SCREEN_PRIVACY_SCREEN_TOAST_VIEW_H_
