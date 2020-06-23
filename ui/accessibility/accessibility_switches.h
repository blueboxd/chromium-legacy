// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Define all the command-line switches used by ui/accessibility.
#ifndef UI_ACCESSIBILITY_ACCESSIBILITY_SWITCHES_H_
#define UI_ACCESSIBILITY_ACCESSIBILITY_SWITCHES_H_

#include "build/build_config.h"
#include "ui/accessibility/ax_base_export.h"

namespace switches {

AX_BASE_EXPORT extern const char kEnableExperimentalAccessibilityAutoclick[];
AX_BASE_EXPORT extern const char
    kEnableExperimentalAccessibilityLabelsDebugging[];
AX_BASE_EXPORT extern const char
    kEnableExperimentalAccessibilityLanguageDetection[];
AX_BASE_EXPORT extern const char
    kEnableExperimentalAccessibilityLanguageDetectionDynamic[];
AX_BASE_EXPORT extern const char kEnableExperimentalAccessibilitySwitchAccess[];
AX_BASE_EXPORT extern const char
    kEnableExperimentalAccessibilitySwitchAccessText[];
AX_BASE_EXPORT extern const char
    kEnableExperimentalAccessibilityChromeVoxAnnotations[];
AX_BASE_EXPORT extern const char
    kDisableExperimentalAccessibilityChromeVoxLanguageSwitching[];
AX_BASE_EXPORT extern const char
    kDisableExperimentalAccessibilityChromeVoxSearchMenus[];
AX_BASE_EXPORT extern const char
    kEnableExperimentalAccessibilityChromeVoxTutorial[];

// Returns true if experimental accessibility language detection is enabled.
AX_BASE_EXPORT bool IsExperimentalAccessibilityLanguageDetectionEnabled();

// Returns true if experimental accessibility language detection support for
// dynamic content is enabled.
AX_BASE_EXPORT bool
IsExperimentalAccessibilityLanguageDetectionDynamicEnabled();

// Returns true if experimental accessibility Switch Access text is enabled.
AX_BASE_EXPORT bool IsExperimentalAccessibilitySwitchAccessTextEnabled();

#if defined(OS_WIN)
AX_BASE_EXPORT extern const char kEnableExperimentalUIAutomation[];
#endif

// Returns true if experimental support for UIAutomation is enabled.
AX_BASE_EXPORT bool IsExperimentalAccessibilityPlatformUIAEnabled();

}  // namespace switches

#endif  // UI_ACCESSIBILITY_ACCESSIBILITY_SWITCHES_H_
