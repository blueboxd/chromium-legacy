// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_INVALIDATION_SWITCHES_H
#define COMPONENTS_INVALIDATION_IMPL_INVALIDATION_SWITCHES_H

#include "base/feature_list.h"

namespace invalidation {
namespace switches {

extern const base::Feature kFCMInvalidationsConservativeEnabling;
extern const base::Feature kFCMInvalidationsStartOnceActiveAccountAvailable;
extern const base::Feature kFCMInvalidationsForSyncDontCheckVersion;
extern const base::Feature kTiclInvalidationsStartInvalidatorOnActiveHandler;

}  // namespace switches
}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_INVALIDATION_SWITCHES_H
