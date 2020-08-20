// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_PHONE_MODEL_TEST_UTIL_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_PHONE_MODEL_TEST_UTIL_H_

#include "chromeos/components/phonehub/phone_status_model.h"

namespace chromeos {
namespace phonehub {

extern const char kFakeMobileProviderName[];

// Creates fake data for use in tests.
const PhoneStatusModel::MobileConnectionMetadata&
CreateFakeMobileConnectionMetadata();
const PhoneStatusModel& CreateFakePhoneStatusModel();

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_PHONE_MODEL_TEST_UTIL_H_
