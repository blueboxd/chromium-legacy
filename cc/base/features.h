// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_FEATURES_H_
#define CC_BASE_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "cc/base/base_export.h"

namespace features {

CC_BASE_EXPORT extern const base::Feature kImpulseScrollAnimations;
CC_BASE_EXPORT extern const base::Feature kTextureLayerSkipWaitForActivation;

#if !defined(OS_ANDROID)
CC_BASE_EXPORT extern const base::Feature kImplLatencyRecovery;
CC_BASE_EXPORT extern const base::Feature kMainLatencyRecovery;
#endif  // !defined(OS_ANDROID)

CC_BASE_EXPORT bool IsImplLatencyRecoveryEnabled();
CC_BASE_EXPORT bool IsMainLatencyRecoveryEnabled();

}  // namespace features

#endif  // CC_BASE_FEATURES_H_
