// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Define all the base::Features used by ui/accessibility.
#ifndef UI_ACCESSIBILITY_ACCESSIBILITY_FEATURES_H_
#define UI_ACCESSIBILITY_ACCESSIBILITY_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_export.h"

namespace features {

AX_EXPORT extern const base::Feature kEnableAccessibilityExposeARIAAnnotations;

// Returns true if ARIA annotations should be exposed to the browser AX Tree.
AX_EXPORT bool IsAccessibilityExposeARIAAnnotationsEnabled();

AX_EXPORT extern const base::Feature kEnableAccessibilityExposeDisplayNone;

// Returns true if "display: none" nodes should be exposed to the
// browser process AXTree.
AX_EXPORT bool IsAccessibilityExposeDisplayNoneEnabled();

AX_EXPORT extern const base::Feature kEnableAccessibilityExposeHTMLElement;

// Returns true if the <html> element should be exposed to the
// browser process AXTree (as an ignored node).
AX_EXPORT bool IsAccessibilityExposeHTMLElementEnabled();

// Serializes accessibility information from the Views tree and deserializes it
// into an AXTree in the browser process.
AX_EXPORT extern const base::Feature kEnableAccessibilityTreeForViews;

// Returns true if the Views tree is exposed using an AXTree in the browser
// process. Returns false if the Views tree is exposed to accessibility
// directly.
AX_EXPORT bool IsAccessibilityTreeForViewsEnabled();

AX_EXPORT extern const base::Feature kAccessibilityFocusHighlight;

// Returns true if the accessibility focus highlight feature is enabled,
// which draws a visual highlight around the focused element on the page
// briefly whenever focus changes.
AX_EXPORT bool IsAccessibilityFocusHighlightEnabled();

#if defined(OS_WIN)
// Enables an experimental Chrome-specific accessibility COM API
AX_EXPORT extern const base::Feature kIChromeAccessible;

// Returns true if the IChromeAccessible COM API is enabled.
AX_EXPORT bool IsIChromeAccessibleEnabled();

#endif  // defined(OS_WIN)

}  // namespace features

#endif  // UI_ACCESSIBILITY_ACCESSIBILITY_FEATURES_H_
