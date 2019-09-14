// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/views_features.h"

#include "base/feature_list.h"

namespace views {
namespace features {

// Please keep alphabetized.

// Increases corner radius on Dialogs for the material design refresh.
// TODO(sajadm): Remove this feature flag when platform inconsistencies
// have been fixed as recorded on: https://crbug.com/932970
const base::Feature kEnableMDRoundedCornersOnDialogs{
    "EnableMDRoundedCornersOnDialogs", base::FEATURE_DISABLED_BY_DEFAULT};

// Only paint views that are invalidated/dirty (i.e. a paint was directly
// scheduled on those views) as opposed to painting all views that intersect
// an invalid rectangle on the layer.
const base::Feature kEnableViewPaintOptimization{
    "EnableViewPaintOptimization", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace views
