// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/views_features.h"

#include "base/feature_list.h"

namespace views {
namespace features {

// Please keep alphabetized.

// Increase corner radius on Dialogs for the material design refresh.
// TODO(tluk): Remove this feature flag when platform inconsistencies
// have been fixed as recorded on: https://crbug.com/932970
const base::Feature kEnableMDRoundedCornersOnDialogs{
    "EnableMDRoundedCornersOnDialogs", base::FEATURE_DISABLED_BY_DEFAULT};

// Use a high-contrast style for ink drops when in platform high-contrast mode,
// including full opacity and a high-contrast color
const base::Feature kEnablePlatformHighContrastInkDrop{
    "EnablePlatformHighContrastInkDrop", base::FEATURE_DISABLED_BY_DEFAULT};

// Only paint views that are invalidated/dirty (i.e. a paint was directly
// scheduled on those views) as opposed to painting all views that intersect
// an invalid rectangle on the layer.
const base::Feature kEnableViewPaintOptimization{
    "EnableViewPaintOptimization", base::FEATURE_DISABLED_BY_DEFAULT};

// Change views::Textfield to take focus on a completed tap, rather than
// immediately on tap down. This only affects touch input. See
// https://crbug.com/1069634.
const base::Feature kTextfieldFocusOnTapUp{"TextfieldFocusOnTapUp",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace views
