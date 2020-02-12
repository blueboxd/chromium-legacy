// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/invalidation_switches.h"

namespace invalidation {
namespace switches {

namespace {

// Default TTL (if the SyncInstanceIDTokenTTL/PolicyInstanceIDTokenTTL feature
// is enabled) is 4 weeks. Exposed for testing.
const int kDefaultInstanceIDTokenTTLSeconds = 28 * 24 * 60 * 60;

}  // namespace

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

const base::Feature kSyncInstanceIDTokenTTL{"SyncInstanceIDTokenTTL",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<int> kSyncInstanceIDTokenTTLSeconds{
    &kSyncInstanceIDTokenTTL, "time_to_live_seconds",
    kDefaultInstanceIDTokenTTLSeconds};

const base::Feature kPolicyInstanceIDTokenTTL{
    "PolicyInstanceIDTokenTTL", base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<int> kPolicyInstanceIDTokenTTLSeconds{
    &kPolicyInstanceIDTokenTTL, "time_to_live_seconds",
    kDefaultInstanceIDTokenTTLSeconds};

}  // namespace switches
}  // namespace invalidation
