// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/features.h"

#include "build/build_config.h"

namespace features {

// Enables impulse-style scroll animations in place of the default ones.
const base::Feature kImpulseScrollAnimations = {
    "ImpulseScrollAnimations", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled BeginMainFrame does not wait for activation in the compositor
// thread for texture layers (crbug.com/1046463)
const base::Feature kTextureLayerSkipWaitForActivation{
    "TextureLayerSkipWaitForActivation", base::FEATURE_DISABLED_BY_DEFAULT};

#if !defined(OS_ANDROID)
// Enables latency recovery on the impl thread.
const base::Feature kImplLatencyRecovery = {"ImplLatencyRecovery",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Enables latency recovery on the main thread.
const base::Feature kMainLatencyRecovery = {"MainLatencyRecovery",
                                            base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // !defined(OS_ANDROID)

bool IsImplLatencyRecoveryEnabled() {
#if defined(OS_ANDROID)
  // TODO(crbug.com/933846): LatencyRecovery is causing jank on Android. Disable
  // for now, with plan to disable more widely on all platforms.
  return false;
#else
  return base::FeatureList::IsEnabled(kImplLatencyRecovery);
#endif
}

bool IsMainLatencyRecoveryEnabled() {
#if defined(OS_ANDROID)
  // TODO(crbug.com/933846): LatencyRecovery is causing jank on Android. Disable
  // for now, with plan to disable more widely on all platforms.
  return false;
#else
  return base::FeatureList::IsEnabled(kMainLatencyRecovery);
#endif
}

}  // namespace features
