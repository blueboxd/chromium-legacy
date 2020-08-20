// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/phone_model_test_util.h"

#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"

namespace chromeos {
namespace phonehub {

const char kFakeMobileProviderName[] = "Fake Mobile Provider";

const PhoneStatusModel::MobileConnectionMetadata&
CreateFakeMobileConnectionMetadata() {
  static const base::NoDestructor<PhoneStatusModel::MobileConnectionMetadata>
      fake_mobile_connection_metadata{
          {PhoneStatusModel::SignalStrength::kFourBars,
           base::UTF8ToUTF16(kFakeMobileProviderName)}};
  return *fake_mobile_connection_metadata;
}

const PhoneStatusModel& CreateFakePhoneStatusModel() {
  static const base::NoDestructor<PhoneStatusModel> fake_status_model{
      PhoneStatusModel::MobileStatus::kSimWithReception,
      CreateFakeMobileConnectionMetadata(),
      PhoneStatusModel::ChargingState::kNotCharging,
      PhoneStatusModel::BatterySaverState::kOff,
      /*battery_percentage=*/100u};
  return *fake_status_model;
}

}  // namespace phonehub
}  // namespace chromeos
