// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_AUDIO_MIC_GAIN_SLIDER_CONTROLLER_H_
#define ASH_SYSTEM_AUDIO_MIC_GAIN_SLIDER_CONTROLLER_H_

#include "ash/system/unified/unified_slider_view.h"

namespace ash {

class MicGainSliderView;

// Controller for mic gain sliders situated in audio detailed
// view in the system tray.
class MicGainSliderController : public UnifiedSliderListener {
 public:
  MicGainSliderController();

  // Create a slider view for a specific input device.
  std::unique_ptr<MicGainSliderView> CreateMicGainSlider(uint64_t device_id);

  // UnifiedSliderListener:
  views::View* CreateView() override;
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;
  void SliderValueChanged(views::Slider* sender,
                          float value,
                          float old_value,
                          views::SliderChangeReason reason) override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_AUDIO_MIC_GAIN_SLIDER_CONTROLLER_H_
