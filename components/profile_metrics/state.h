// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROFILE_METRICS_STATE_H_
#define COMPONENTS_PROFILE_METRICS_STATE_H_

namespace profile_metrics {

// State for a profile avatar, documenting what Chrome UI exactly shows.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AvatarState {
  // All SignedIn* states denote having a primary account (incl. unconsented,
  // not necessarily syncing).
  kSignedInGaia =
      0,  // User has the avatar from GAIA (the default for signed-in users).
  kSignedInModern = 1,    // User has explicitly selected a modern avatar.
  kSignedInOld = 2,       // User has explicitly selected an old avatar.
  kSignedOutDefault = 3,  // Grey silhouette.
  kSignedOutModern = 4,   // User has explicitly selected a modern avatar.
  kSignedOutOld = 5,      // User has explicitly selected an old avatar.
  kMaxValue = kSignedOutOld
};

// State for a profile name, documenting what Chrome UI exactly shows.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class NameState {
  kGaiaName = 0,            // The name of the user from Gaia.
  kGaiaAndCustomName = 1,   // The name of the user from Gaia and the custom
                            // local name specified by the user.
  kGaiaAndDefaultName = 2,  // Chrome shows "Person X" alongside the Gaia name
                            // because it is needed to resolve ambiguity.
  kCustomName = 3,   // Only a custom name of the profile specified by the user.
  kDefaultName = 4,  // Only "Person X" since there's nothing better.
  kMaxValue = kDefaultName
};

// Type of the unconsented primary account in a profile.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class UnconsentedPrimaryAccountType {
  kConsumer = 0,
  kEnterprise = 1,
  kChild = 2,
  kSignedOut = 3,
  kMaxValue = kSignedOut
};

// Different types of reporting for profile state. This is used as a histogram
// suffix.
enum class StateSuffix {
  kAll,                 // Recorded for all clients and all their profiles.
  kActiveMultiProfile,  // Recorded for multi-profile users with >=2 active
                        // profiles, for all their profiles.
  kLatentMultiProfile,  // Recorded for multi-profile users with one active
                        // profile, for all their profiles.
  kLatentMultiProfileActive,  // Recorded for multi-profile users with one
                              // active profile, only for the active profile.
  kLatentMultiProfileOthers,  // Recorded for multi-profile users with one
                              // active profile, only for the non-active
                              // profiles.
  kSingleProfile  // Recorded for single-profile users for their single profile.
};

// Records the state of profile's avatar.
void LogProfileAvatar(AvatarState avatar_state, StateSuffix suffix);

// Records the state of profile's name.
void LogProfileName(NameState name_state, StateSuffix suffix);

// Records the state of profile's UPA.
void LogProfileAccountType(UnconsentedPrimaryAccountType account_type,
                           StateSuffix suffix);

// Records the days since last use of a profile.
void LogProfileDaysSinceLastUse(int days_since_last_use, StateSuffix suffix);

}  // namespace profile_metrics

#endif  // COMPONENTS_PROFILE_METRICS_STATE_H_
