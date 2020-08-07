// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_TYPE_VIEW_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_TYPE_VIEW_H_

#include "ui/views/controls/button/button.h"

namespace ash {

class CaptureModeToggleButton;

// A view that is part of the CaptureBarView, from which the user can toggle
// between the two available capture types (image, and video).
class CaptureModeTypeView : public views::View, public views::ButtonListener {
 public:
  CaptureModeTypeView();
  CaptureModeTypeView(const CaptureModeTypeView&) = delete;
  CaptureModeTypeView& operator=(const CaptureModeTypeView&) = delete;
  ~CaptureModeTypeView() override;

  // views::View:
  const char* GetClassName() const override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  // Owned by the views hierarchy.
  CaptureModeToggleButton* image_toggle_button_;
  CaptureModeToggleButton* video_toggle_button_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_TYPE_VIEW_H_
