// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/invalidation_switches.h"

namespace invalidation {
namespace switches {

const base::Feature kFCMInvalidationsConservativeEnabling = {
    "FCMInvalidationsConservativeEnabling", base::FEATURE_ENABLED_BY_DEFAULT};

// This feature affects only Android.
const base::Feature kFCMInvalidationsStartOnceActiveAccountAvailable = {
    "FCMInvalidationsStartOnceActiveAccountAvailable",
    base::FEATURE_ENABLED_BY_DEFAULT};

extern const base::Feature kFCMInvalidationsForSyncDontCheckVersion;
const base::Feature kFCMInvalidationsForSyncDontCheckVersion = {
    "FCMInvalidationsForSyncDontCheckVersion",
    base::FEATURE_ENABLED_BY_DEFAULT};

// TODO(melandory): Once FCM invalidations are launched, this feature toggle
// should be removed.
// TODO(crbug.com/964296): Re-enable when bug is resolved.
const base::Feature kTiclInvalidationsStartInvalidatorOnActiveHandler = {
    "TiclInvalidationsStartInvalidatorOnActiveHandler",
    base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace switches
}  // namespace invalidation
