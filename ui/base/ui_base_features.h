// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_UI_BASE_FEATURES_H_
#define UI_BASE_UI_BASE_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "ui/base/buildflags.h"

namespace features {

// Keep sorted!

COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kColorProviderRedirection;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kCompositorThreadedScrollbarScrolling;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kExperimentalFlingAnimation;
#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kSettingsShowsPerKeyboardSettings;
#endif  // defined(OS_CHROMEOS)
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kInputMethodSettingsUiUpdate;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kPercentBasedScrolling;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kPointerLockOptions;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kSystemCaptionStyle;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kSystemKeyboardLock;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kNotificationIndicator;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kUiCompositorScrollWithLayers;

COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsNotificationIndicatorEnabled();

COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsUiGpuRasterizationEnabled();

#if defined(OS_WIN)
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kCalculateNativeWinOcclusion;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kInputPaneOnScreenKeyboard;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kPointerEventsForTouch;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kPrecisionTouchpadLogging;
COMPONENT_EXPORT(UI_BASE_FEATURES) extern const base::Feature kTSFImeSupport;

// Returns true if the system should use WM_POINTER events for touch events.
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsUsingWMPointerForTouch();
#endif  // defined(OS_WIN)

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kDirectManipulationStylus;
#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)

// Used to enable forced colors mode for web content.
COMPONENT_EXPORT(UI_BASE_FEATURES) extern const base::Feature kForcedColors;
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsForcedColorsEnabled();

// Used to enable the eye-dropper in the refresh color-picker.
COMPONENT_EXPORT(UI_BASE_FEATURES) extern const base::Feature kEyeDropper;
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsEyeDropperEnabled();

// Used to enable the new controls UI.
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kFormControlsRefresh;
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsFormControlsRefreshEnabled();

// Used to enable the common select popup.
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kUseCommonSelectPopup;
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsUseCommonSelectPopupEnabled();

// Use mojo communication in the drm platform instead of paramtraits. Remove
// this switch (and associated code) when the drm platform always uses mojo
// communication.
// TODO(rjkroege): Remove in http://crbug.com/806092.
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kEnableOzoneDrmMojo;
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsOzoneDrmMojo();

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kHandwritingGesture;

COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kNewShortcutMapping;

COMPONENT_EXPORT(UI_BASE_FEATURES)
bool IsNewShortcutMappingEnabled();
#endif

COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kWebUIA11yEnhancements;

// Indicates whether DrmOverlayManager should used the synchronous API to
// perform pageflip tests.
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kSynchronousPageFlipTesting;

COMPONENT_EXPORT(UI_BASE_FEATURES)
bool IsSynchronousPageFlipTestingEnabled();

}  // namespace features

#endif  // UI_BASE_UI_BASE_FEATURES_H_
