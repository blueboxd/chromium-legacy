// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/caption_style.h"

#include <Windows.Media.ClosedCaptioning.h>
#include <wrl/client.h>

#include <string>

#include "base/command_line.h"
#include "base/trace_event/trace_event.h"
#include "base/win/core_winrt_util.h"
#include "base/win/windows_version.h"
#include "ui/base/ui_base_features.h"

namespace ui {

using namespace ABI::Windows::Media::ClosedCaptioning;

namespace {

// This is used to add tracing when a COM call fails.
// Param 1 - line is the line number in the caller. This helps locate
// problematic code.
// Param 2 - hr is the failure HRESULT.
void LogCapStyleWinError(int line, HRESULT hr) {
  TRACE_EVENT2("ui", "LogWindowsCaptionStyleError", "line", line, "hr", hr);
}

// Translates a Windows::Media::ClosedCaptioning::ClosedCaptionStyle to a
// CSS FontFamily value.
// These fonts were chosen to satisfy the characteristics represented by values
// of ClosedCaptionStyle Enum in Windows Settings.
void GetFontFamilyString(ClosedCaptionStyle closed_caption_style,
                         std::string* css_font_family,
                         std::string* css_font_variant) {
  *css_font_variant = "normal";
  switch (closed_caption_style) {
    case ClosedCaptionStyle_MonospacedWithSerifs:
      *css_font_family = "Courier New";
      break;
    case ClosedCaptionStyle_ProportionalWithSerifs:
      *css_font_family = "Times New Roman";
      break;
    case ClosedCaptionStyle_MonospacedWithoutSerifs:
      *css_font_family = "Consolas";
      break;
    case ClosedCaptionStyle_ProportionalWithoutSerifs:
      *css_font_family = "Tahoma";
      break;
    case ClosedCaptionStyle_Casual:
      *css_font_family = "Segoe Print";
      break;
    case ClosedCaptionStyle_Cursive:
      *css_font_family = "Segoe Script";
      break;
    case ClosedCaptionStyle_SmallCapitals:
      *css_font_family = "Tahoma";
      *css_font_variant = "small-caps";
      break;
    case ClosedCaptionStyle_Default:
      *css_font_family = std::string();
      break;
  }
}

// Translates a Windows::Media::ClosedCaptioning::ClosedCaptionEdgeEffect to a
// CSS style value.
std::string GetEdgeEffectString(ClosedCaptionEdgeEffect edge_effect) {
  switch (edge_effect) {
    case ClosedCaptionEdgeEffect_None:
      return "none";
    case ClosedCaptionEdgeEffect_Raised:
      return "-1px 0px 0px silver, 0px -1px 0px silver, 1px 1px 0px black, 2px "
             "2px 0px black, 3px 3px 0px black";
    case ClosedCaptionEdgeEffect_Depressed:
      return "1px 1px 0px silver, 0px 1px 0px silver, -1px -1px 0px black, "
             "-1px "
             "0px 0px black";
    case ClosedCaptionEdgeEffect_Uniform:
      return "0px 0px 4px black, 0px 0px 4px black, 0px 0px 4px black, 0px 0px "
             "4px black";
    case ClosedCaptionEdgeEffect_DropShadow:
      return "3px 3px 3px 2px black";
    case ClosedCaptionEdgeEffect_Default:
      return std::string();
  }
}

// Translates a Windows::Media::ClosedCaptioning::ClosedCaptionSize to a CSS
// style value.
std::string GetCaptionSizeString(ClosedCaptionSize caption_size) {
  switch (caption_size) {
    case ClosedCaptionSize_FiftyPercent:
      return "50%";
    case ClosedCaptionSize_OneHundredPercent:
      return "100%";
    case ClosedCaptionSize_OneHundredFiftyPercent:
      return "150%";
    case ClosedCaptionSize_TwoHundredPercent:
      return "200%";
    case ClosedCaptionSize_Default:
      return std::string();
  }
}

// Translates a Windows::Media::ClosedCaptioning::ClosedCaptionColor to a CSS
// color string.
std::string GetCssColor(ClosedCaptionColor caption_color) {
  switch (caption_color) {
    case ClosedCaptionColor_Black:
      return "black";
    case ClosedCaptionColor_Red:
      return "red";
    case ClosedCaptionColor_Green:
      return "green";
    case ClosedCaptionColor_Blue:
      return "blue";
    case ClosedCaptionColor_Yellow:
      return "yellow";
    case ClosedCaptionColor_Magenta:
      return "magenta";
    case ClosedCaptionColor_Cyan:
      return "cyan";
    case ClosedCaptionColor_White:
    case ClosedCaptionColor_Default:
      return "white";
  }
}

base::Optional<CaptionStyle> InitializeFromSystemSettings() {
  DCHECK_GE(base::win::GetVersion(), base::win::Version::WIN10);
  DCHECK(base::FeatureList::IsEnabled(features::kSystemCaptionStyle));

  // Need to do this check before using ScopedHString.
  bool can_use_scoped_hstring =
      base::win::ResolveCoreWinRTDelayload() &&
      base::win::ScopedHString::ResolveCoreWinRTStringDelayload();

  if (!can_use_scoped_hstring) {
    DLOG(ERROR) << "Failed loading functions from combase.dll";
    LogCapStyleWinError(__LINE__, E_FAIL);
    return base::nullopt;
  }

  base::win::ScopedHString closed_caption_properties_string =
      base::win::ScopedHString::Create(
          RuntimeClass_Windows_Media_ClosedCaptioning_ClosedCaptionProperties);
  Microsoft::WRL::ComPtr<IClosedCaptionPropertiesStatics>
      closed_caption_properties_statics;
  HRESULT hr = base::win::RoGetActivationFactory(
      closed_caption_properties_string.get(),
      IID_PPV_ARGS(&closed_caption_properties_statics));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to Get ActivationFactory for ClosedCaptionProperties"
                << ", HRESULT: 0x" << std::hex << hr;
    LogCapStyleWinError(__LINE__, hr);
    return base::nullopt;
  }

  ClosedCaptionSize font_size = ClosedCaptionSize_Default;
  hr = closed_caption_properties_statics->get_FontSize(&font_size);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to retrieve Font Size"
                << ", HRESULT: 0x" << std::hex << hr;
    LogCapStyleWinError(__LINE__, hr);
    return base::nullopt;
  }

  ClosedCaptionEdgeEffect edge_effect = ClosedCaptionEdgeEffect_Default;
  hr = closed_caption_properties_statics->get_FontEffect(&edge_effect);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to retrieve Font Effect"
                << ", HRESULT: 0x" << std::hex << hr;
    LogCapStyleWinError(__LINE__, hr);
    return base::nullopt;
  }

  ClosedCaptionStyle font_family = ClosedCaptionStyle_Default;
  hr = closed_caption_properties_statics->get_FontStyle(&font_family);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to retrieve Font Family"
                << ", HRESULT: 0x" << std::hex << hr;
    LogCapStyleWinError(__LINE__, hr);
    return base::nullopt;
  }

  ClosedCaptionColor font_color = ClosedCaptionColor_Default;
  hr = closed_caption_properties_statics->get_FontColor(&font_color);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to retrieve Font Color"
                << ", HRESULT: 0x" << std::hex << hr;
    LogCapStyleWinError(__LINE__, hr);
    return base::nullopt;
  }

  ClosedCaptionColor background_color = ClosedCaptionColor_Default;
  hr =
      closed_caption_properties_statics->get_BackgroundColor(&background_color);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to retrieve Background Color"
                << ", HRESULT: 0x" << std::hex << hr;
    LogCapStyleWinError(__LINE__, hr);
    return base::nullopt;
  }

  CaptionStyle caption_style;
  GetFontFamilyString(font_family, &(caption_style.font_family),
                      &(caption_style.font_variant));
  caption_style.text_size = GetCaptionSizeString(font_size);
  caption_style.text_shadow = GetEdgeEffectString(edge_effect);
  caption_style.text_color = GetCssColor(font_color);
  caption_style.background_color = GetCssColor(background_color);

  return caption_style;
}

}  // namespace

base::Optional<CaptionStyle> CaptionStyle::FromSystemSettings() {
  if (base::win::GetVersion() >= base::win::Version::WIN10 &&
      base::FeatureList::IsEnabled(features::kSystemCaptionStyle)) {
    return InitializeFromSystemSettings();
  }
  // Return default CaptionStyle for pre Win10 versions since system settings
  // don't allow caption styling.
  return base::nullopt;
}

}  // namespace ui
