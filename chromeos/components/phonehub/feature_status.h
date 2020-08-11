// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_FEATURE_STATUS_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_FEATURE_STATUS_H_

#include <ostream>

namespace chromeos {
namespace phonehub {

// Enum representing potential status values for the Phone Hub feature.
enum class FeatureStatus {
  // The user's devices are not eligible for the feature. This means that either
  // the Chrome OS device or the user's phone (or both) have not enrolled with
  // the requisite feature enum values.
  kNotEligibleForFeature = 0,

  // The user has a phone eligible for the feature, but they have not yet
  // started the opt-in flow.
  kEligiblePhoneButNotSetUp = 1,

  // The user has selected a phone in the opt-in flow, but setup is not yet
  // complete. Note that setting up the feature requires interaction with a
  // server and with the phone itself.
  kPhoneSelectedAndPendingSetup = 2,

  // An enterprise policy has prohibited this feature from running.
  kProhibitedByPolicy = 3,

  // The feature is disabled, but the user could enable it via settings.
  kDisabled = 4,

  // The feature is enabled, but it is currently unavailable because Bluetooth
  // is disabled (the feature cannot run without Bluetooth).
  kUnavailableBluetoothOff = 5,

  // The feature is enabled, but currently there is no active connection to
  // the phone.
  kEnabledButDisconnected = 6,

  // The feature is enabled, and there is an active attempt to connect to the
  // phone.
  kEnabledAndConnecting = 7,

  // The feature is enabled, and there is an active connection with the phone.
  kEnabledAndConnected = 8
};

std::ostream& operator<<(std::ostream& stream, FeatureStatus status);

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_CO  MPONENTS_PHONEHUB_FEATURE_STATUS_H_
