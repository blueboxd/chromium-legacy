// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_source_view.h"

#include <memory>

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_toggle_button.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

CaptureModeSourceView::CaptureModeSourceView()
    : fullscreen_toggle_button_(
          AddChildView(std::make_unique<CaptureModeToggleButton>(
              this,
              kCaptureModeFullscreenIcon))),
      region_toggle_button_(AddChildView(
          std::make_unique<CaptureModeToggleButton>(this,
                                                    kCaptureModeRegionIcon))),
      window_toggle_button_(AddChildView(
          std::make_unique<CaptureModeToggleButton>(this,
                                                    kCaptureModeWindowIcon))) {
  auto* box_layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      capture_mode::kBetweenChildSpacing));
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  OnCaptureSourceChanged(CaptureModeController::Get()->source());
}

CaptureModeSourceView::~CaptureModeSourceView() = default;

void CaptureModeSourceView::OnCaptureSourceChanged(
    CaptureModeSource new_source) {
  fullscreen_toggle_button_->SetToggled(new_source ==
                                        CaptureModeSource::kFullscreen);
  region_toggle_button_->SetToggled(new_source == CaptureModeSource::kRegion);
  window_toggle_button_->SetToggled(new_source == CaptureModeSource::kWindow);
}

const char* CaptureModeSourceView::GetClassName() const {
  return "CaptureModeSourceView";
}

void CaptureModeSourceView::ButtonPressed(views::Button* sender,
                                          const ui::Event& event) {
  auto* controller = CaptureModeController::Get();
  if (sender == fullscreen_toggle_button_) {
    controller->SetSource(CaptureModeSource::kFullscreen);
  } else if (sender == region_toggle_button_) {
    controller->SetSource(CaptureModeSource::kRegion);
  } else {
    DCHECK_EQ(sender, window_toggle_button_);
    controller->SetSource(CaptureModeSource::kWindow);
  }
}

}  // namespace ash
