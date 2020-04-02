// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/mic_gain_slider_controller.h"

#include "ash/shell.h"
#include "ash/system/audio/mic_gain_slider_view.h"

using chromeos::CrasAudioHandler;

namespace ash {

MicGainSliderController::MicGainSliderController() = default;

std::unique_ptr<MicGainSliderView> MicGainSliderController::CreateMicGainSlider(
    uint64_t device_id) {
  return std::make_unique<MicGainSliderView>(this, device_id);
}

views::View* MicGainSliderController::CreateView() {
  return nullptr;
}

void MicGainSliderController::ButtonPressed(views::Button* sender,
                                            const ui::Event& event) {
  bool is_muted = !CrasAudioHandler::Get()->IsInputMuted();

  // TODO(amehfooz): Add metrics and logging.

  CrasAudioHandler::Get()->SetMuteForDevice(
      CrasAudioHandler::Get()->GetPrimaryActiveInputNode(), is_muted);
}

void MicGainSliderController::SliderValueChanged(
    views::Slider* sender,
    float value,
    float old_value,
    views::SliderChangeReason reason) {
  if (reason != views::SliderChangeReason::kByUser)
    return;

  CrasAudioHandler::Get()->SetInputGainPercent(value * 100);
}

}  // namespace ash
