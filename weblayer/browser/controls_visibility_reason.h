// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_CONTROLS_VISIBILITY_REASON_H_
#define WEBLAYER_BROWSER_CONTROLS_VISIBILITY_REASON_H_

namespace weblayer {

// This enum represents actions or UI conditions that affect the visibility of
// top UI, and is used to track concurrent concerns and to allow native and Java
// code to coordinate.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.weblayer_private
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: ImplControlsVisibilityReason
enum class ControlsVisibilityReason {
  // Browser controls are hidden when fullscreen is active.
  kFullscreen = 0,

  // Browser controls are always shown for a few seconds after a navigation.
  kPostNavigation,

  // Find in page forces browser controls to be visible.
  kFindInPage,

  // Tab modal dialogs obscure the content while leaving controls interactive.
  kTabModalDialog,

  kReasonCount,
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_CONTROLS_VISIBILITY_REASON_H_
