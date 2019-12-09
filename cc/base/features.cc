// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/features.h"

namespace features {
// Enables impulse-style scroll animations in place of the default ones.
const base::Feature kImpulseScrollAnimations = {
    "ImpulseScrollAnimations", base::FEATURE_DISABLED_BY_DEFAULT};
}  // namespace features
