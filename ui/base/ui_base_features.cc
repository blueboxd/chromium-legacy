// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ui_base_features.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace features {

#if defined(OS_WIN)
// If enabled, calculate native window occlusion - Windows-only.
const base::Feature kCalculateNativeWinOcclusion{
    "CalculateNativeWinOcclusion", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // OW_WIN

// Whether or not to delegate color queries to the color provider.
const base::Feature kColorProviderRedirection = {
    "ColorProviderRedirection", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_CHROMEOS)
// Integrate input method specific settings to Chrome OS settings page.
// https://crbug.com/895886.
const base::Feature kSettingsShowsPerKeyboardSettings = {
    "InputMethodIntegratedSettings", base::FEATURE_DISABLED_BY_DEFAULT};

// Experimental shortcut handling and mapping to address i18n issues.
// https://crbug.com/1067269
const base::Feature kNewShortcutMapping = {"NewShortcutMapping",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

bool IsNewShortcutMappingEnabled() {
  return base::FeatureList::IsEnabled(kNewShortcutMapping);
}
#endif  // defined(OS_CHROMEOS)

// Update of the virtual keyboard settings UI as described in
// https://crbug.com/876901.
const base::Feature kInputMethodSettingsUiUpdate = {
    "InputMethodSettingsUiUpdate", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables percent-based scrolling for mousewheel and keyboard initiated
// scrolls.
const base::Feature kPercentBasedScrolling = {
    "PercentBasedScrolling", base::FEATURE_DISABLED_BY_DEFAULT};

// Allows requesting unadjusted movement when entering pointerlock.
const base::Feature kPointerLockOptions = {"PointerLockOptions",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Allows system caption style for WebVTT Captions.
const base::Feature kSystemCaptionStyle{"SystemCaptionStyle",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Allows system keyboard event capture via the keyboard lock API.
const base::Feature kSystemKeyboardLock{"SystemKeyboardLock",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kNotificationIndicator = {
    "EnableNotificationIndicator", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsNotificationIndicatorEnabled() {
  return base::FeatureList::IsEnabled(kNotificationIndicator);
}

// Enables GPU rasterization for all UI drawing (where not blacklisted).
const base::Feature kUiGpuRasterization = {"UiGpuRasterization",
#if defined(OS_MACOSX) || defined(OS_CHROMEOS) || defined(OS_FUCHSIA)
                                           base::FEATURE_ENABLED_BY_DEFAULT
#else
                                           base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

bool IsUiGpuRasterizationEnabled() {
  return base::FeatureList::IsEnabled(kUiGpuRasterization);
}

// Enables scrolling with layers under ui using the ui::Compositor.
const base::Feature kUiCompositorScrollWithLayers = {
    "UiCompositorScrollWithLayers",
// TODO(https://crbug.com/615948): Use composited scrolling on all platforms.
#if defined(OS_MACOSX)
    base::FEATURE_ENABLED_BY_DEFAULT
#else
    base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Enables compositor threaded scrollbar scrolling by mapping pointer events to
// gesture events.
const base::Feature kCompositorThreadedScrollbarScrolling = {
    "CompositorThreadedScrollbarScrolling", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the use of a touch fling curve that is based on the behavior of
// native apps on Windows.
const base::Feature kExperimentalFlingAnimation{
    "ExperimentalFlingAnimation", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_WIN)
// Enables InputPane API for controlling on screen keyboard.
const base::Feature kInputPaneOnScreenKeyboard = {
    "InputPaneOnScreenKeyboard", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables using WM_POINTER instead of WM_TOUCH for touch events.
const base::Feature kPointerEventsForTouch = {"PointerEventsForTouch",
                                              base::FEATURE_ENABLED_BY_DEFAULT};
// Enables using TSF (over IMM32) for IME.
const base::Feature kTSFImeSupport = {"TSFImeSupport",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

bool IsUsingWMPointerForTouch() {
  return base::win::GetVersion() >= base::win::Version::WIN8 &&
         base::FeatureList::IsEnabled(kPointerEventsForTouch);
}

// Enables Logging for DirectManipulation.
const base::Feature kPrecisionTouchpadLogging{
    "PrecisionTouchpadLogging", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_WIN)

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
// Enables stylus appearing as touch when in contact with digitizer.
const base::Feature kDirectManipulationStylus = {
    "DirectManipulationStylus",
#if defined(OS_WIN)
    base::FEATURE_ENABLED_BY_DEFAULT
#else
    base::FEATURE_DISABLED_BY_DEFAULT
#endif
};
#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)

// Enables forced colors mode for web content.
const base::Feature kForcedColors{"ForcedColors",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

bool IsForcedColorsEnabled() {
  static const bool forced_colors_enabled =
      base::FeatureList::IsEnabled(features::kForcedColors);
  return forced_colors_enabled;
}

// Enables the eye-dropper in the refresh color-picker.
const base::Feature kEyeDropper{"EyeDropper",
                                base::FEATURE_DISABLED_BY_DEFAULT};

bool IsEyeDropperEnabled() {
  return IsFormControlsRefreshEnabled() &&
         base::FeatureList::IsEnabled(features::kEyeDropper);
}

// Enable the FormControlsRefresh feature for Windows, ChromeOS, Linux, and Mac.
// This feature will be released for Android in later milestones. See
// crbug.com/1012106 for the Windows launch bug, and crbug.com/1012108 for the
// Mac launch bug.
const base::Feature kFormControlsRefresh = {"FormControlsRefresh",
#if defined(OS_WIN) || defined(OS_CHROMEOS) || defined(OS_LINUX) || \
    defined(OS_MACOSX)
                                            base::FEATURE_ENABLED_BY_DEFAULT
#else
                                            base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

bool IsFormControlsRefreshEnabled() {
  static const bool form_controls_refresh_enabled =
      base::FeatureList::IsEnabled(features::kFormControlsRefresh);
  return form_controls_refresh_enabled;
}

// Enable the common select popup.
const base::Feature kUseCommonSelectPopup = {"UseCommonSelectPopup",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

bool IsUseCommonSelectPopupEnabled() {
  return base::FeatureList::IsEnabled(features::kUseCommonSelectPopup);
}

// Enable WebUI accessibility enhancements for review and testing.
const base::Feature kWebUIA11yEnhancements{"WebUIA11yEnhancements",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableOzoneDrmMojo = {"OzoneDrmMojo",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

bool IsOzoneDrmMojo() {
  return base::FeatureList::IsEnabled(kEnableOzoneDrmMojo);
}

#if defined(OS_CHROMEOS)
const base::Feature kHandwritingGesture = {"HandwritingGesture",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
#endif

const base::Feature kSynchronousPageFlipTesting{
    "SynchronousPageFlipTesting", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsSynchronousPageFlipTestingEnabled() {
  return base::FeatureList::IsEnabled(kSynchronousPageFlipTesting);
}

}  // namespace features
