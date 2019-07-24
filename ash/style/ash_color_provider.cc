// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/ash_color_provider.h"

#include "ash/public/cpp/ash_switches.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

namespace ash {

namespace {

// Opacity of the light/dark ink ripple.
constexpr float kLightInkRippleOpacity = 0.08f;
constexpr float kDarkInkRippleOpacity = 0.06f;

// Gets the color mode value from feature flag "--ash-color-mode".
AshColorProvider::AshColorMode GetColorModeFromCommandLine() {
  const base::CommandLine* cl = base::CommandLine::ForCurrentProcess();

  if (!cl->HasSwitch(switches::kAshColorMode))
    return AshColorProvider::AshColorMode::kDefault;

  const std::string switch_value =
      cl->GetSwitchValueASCII(switches::kAshColorMode);
  if (switch_value == switches::kAshColorModeDark)
    return AshColorProvider::AshColorMode::kDark;

  if (switch_value == switches::kAshColorModeLight)
    return AshColorProvider::AshColorMode::kLight;

  return AshColorProvider::AshColorMode::kDefault;
}

}  // namespace

AshColorProvider::AshColorProvider()
    : color_mode_(GetColorModeFromCommandLine()) {}

AshColorProvider::~AshColorProvider() = default;

SkColor AshColorProvider::GetShieldLayerColor(ShieldLayerType type) const {
  SkColor light_color, dark_color;
  switch (type) {
    case ShieldLayerType::kAlpha20:
      light_color = SkColorSetA(SK_ColorWHITE, 0x33);  // 20%
      dark_color = SkColorSetA(gfx::kGoogleGrey900, 0x33);
      break;
    case ShieldLayerType::kAlpha40:
      light_color = SkColorSetA(SK_ColorWHITE, 0x66);  // 40%
      dark_color = SkColorSetA(gfx::kGoogleGrey900, 0x66);
      break;
    case ShieldLayerType::kAlpha60:
      light_color = SkColorSetA(SK_ColorWHITE, 0x99);  // 60%
      dark_color = SkColorSetA(gfx::kGoogleGrey900, 0x99);
      break;
  }
  return SelectColorOnMode(light_color, dark_color);
}

SkColor AshColorProvider::GetBaseLayerColor(BaseLayerType type) const {
  SkColor light_color, dark_color;
  switch (type) {
    case BaseLayerType::kTransparentWithBlur:
      light_color = SkColorSetA(SK_ColorWHITE, 0xBC);  // 74%
      dark_color = SkColorSetA(gfx::kGoogleGrey900, 0xBC);
      break;
    case BaseLayerType::kTransparentWithoutBlur:
      light_color = SkColorSetA(SK_ColorWHITE, 0xE6);  // 90%
      dark_color = SkColorSetA(gfx::kGoogleGrey900, 0xE6);
      break;
    case BaseLayerType::kOpaque:
      light_color = SK_ColorWHITE;
      dark_color = gfx::kGoogleGrey900;
      break;
  }
  return SelectColorOnMode(light_color, dark_color);
}

SkColor AshColorProvider::GetControlsLayerColor(ControlsLayerType type) const {
  SkColor light_color, dark_color;
  switch (type) {
    case ControlsLayerType::kHairlineBorder:
    case ControlsLayerType::kSeparator:
      light_color = SkColorSetA(SK_ColorBLACK, 0x24);  // 14%
      dark_color = SkColorSetA(SK_ColorWHITE, 0x24);
      break;
    case ControlsLayerType::kInactiveControlBackground:
      light_color = SkColorSetA(SK_ColorBLACK, 0x0D);  // 5%
      dark_color = SkColorSetA(SK_ColorWHITE, 0x1A);   // 10%
      break;
    case ControlsLayerType::kActiveControlBackground:
    case ControlsLayerType::kFocusRing:
      light_color = gfx::kGoogleBlue600;
      dark_color = gfx::kGoogleBlue300;
      break;
  }
  return SelectColorOnMode(light_color, dark_color);
}

AshColorProvider::RippleAttributes AshColorProvider::GetRippleAttributes(
    SkColor bg_color) const {
  const SkColor base_color = color_utils::GetColorWithMaxContrast(bg_color);
  const float opacity = color_utils::IsDark(base_color)
                            ? kDarkInkRippleOpacity
                            : kLightInkRippleOpacity;
  return RippleAttributes(base_color, opacity, opacity);
}

SkColor AshColorProvider::SelectColorOnMode(SkColor light_color,
                                            SkColor dark_color) const {
  if (color_mode_ == AshColorMode::kLight)
    return light_color;
  if (color_mode_ == AshColorMode::kDark)
    return dark_color;

  LOG(ERROR) << "Current color mode is AshColorMode::kDefault and should not "
             << "retrieve color from AshColorProvider.";
  return SK_ColorTRANSPARENT;
}

}  // namespace ash
