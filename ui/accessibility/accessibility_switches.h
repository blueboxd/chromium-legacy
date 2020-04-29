// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Define all the command-line switches used by ui/accessibility.
#ifndef UI_ACCESSIBILITY_ACCESSIBILITY_SWITCHES_H_
#define UI_ACCESSIBILITY_ACCESSIBILITY_SWITCHES_H_

#include "build/build_config.h"
#include "ui/accessibility/ax_export.h"

namespace switches {

AX_EXPORT extern const char kEnableExperimentalAccessibilityAutoclick[];
AX_EXPORT extern const char kEnableExperimentalAccessibilityLabelsDebugging[];
AX_EXPORT extern const char kEnableExperimentalAccessibilityLanguageDetection[];
AX_EXPORT extern const char
    kEnableExperimentalAccessibilityLanguageDetectionDynamic[];
AX_EXPORT extern const char kEnableExperimentalAccessibilitySwitchAccess[];
AX_EXPORT extern const char kEnableExperimentalAccessibilitySwitchAccessText[];
AX_EXPORT extern const char
    kEnableExperimentalAccessibilityChromeVoxAnnotations[];
AX_EXPORT extern const char
    kDisableExperimentalAccessibilityChromeVoxLanguageSwitching[];
AX_EXPORT extern const char
    kDisableExperimentalAccessibilityChromeVoxSearchMenus[];
AX_EXPORT extern const char kEnableExperimentalAccessibilityChromeVoxTutorial[];

// Returns true if experimental accessibility language detection is enabled.
AX_EXPORT bool IsExperimentalAccessibilityLanguageDetectionEnabled();

// Returns true if experimental accessibility language detection support for
// dynamic content is enabled.
AX_EXPORT bool IsExperimentalAccessibilityLanguageDetectionDynamicEnabled();

// Returns true if experimental accessibility Switch Access text is enabled.
AX_EXPORT bool IsExperimentalAccessibilitySwitchAccessTextEnabled();

#if defined(OS_WIN)
AX_EXPORT extern const char kEnableExperimentalUIAutomation[];
#endif

// Returns true if experimental support for UIAutomation is enabled.
AX_EXPORT bool IsExperimentalAccessibilityPlatformUIAEnabled();

}  // namespace switches

#endif  // UI_ACCESSIBILITY_ACCESSIBILITY_SWITCHES_H_
