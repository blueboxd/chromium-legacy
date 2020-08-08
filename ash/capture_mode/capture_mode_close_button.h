// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_CLOSE_BUTTON_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_CLOSE_BUTTON_H_

#include "ash/ash_export.h"
#include "ash/capture_mode/view_with_ink_drop.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"

namespace ash {

// A view that shows a close button which is part of the CaptureBarView.
class ASH_EXPORT CaptureModeCloseButton
    : public ViewWithInkDrop<views::ImageButton> {
 public:
  explicit CaptureModeCloseButton(views::ButtonListener* listener);
  CaptureModeCloseButton(const CaptureModeCloseButton&) = delete;
  CaptureModeCloseButton& operator=(const CaptureModeCloseButton&) = delete;
  ~CaptureModeCloseButton() override = default;

  // views::ImageButton:
  const char* GetClassName() const override;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_CLOSE_BUTTON_H_
