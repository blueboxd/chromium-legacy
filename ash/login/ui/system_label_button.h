// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_SYSTEM_LABEL_BUTTON_H_
#define ASH_LOGIN_UI_SYSTEM_LABEL_BUTTON_H_

#include "ash/ash_export.h"
#include "ui/views/controls/button/label_button.h"

namespace ash {

// SystemLabelButton provides styled buttons with label for the login screen.
class ASH_EXPORT SystemLabelButton : public views::LabelButton {
 public:
  enum class DisplayType { DEFAULT, ALERT_NO_ICON, ALERT_WITH_ICON };

  SystemLabelButton(views::ButtonListener* listener,
                    const base::string16& text,
                    DisplayType display_type);
  SystemLabelButton(const SystemLabelButton&) = delete;
  SystemLabelButton& operator=(const SystemLabelButton&) = delete;
  ~SystemLabelButton() override = default;

  // views::LabelButton:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  gfx::Insets GetInsets() const override;

 private:
  SkColor background_color_ = SK_ColorGREEN;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_SYSTEM_LABEL_BUTTON_H_
