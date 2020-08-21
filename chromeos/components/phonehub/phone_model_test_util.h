// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_PHONE_MODEL_TEST_UTIL_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_PHONE_MODEL_TEST_UTIL_H_

#include "chromeos/components/phonehub/browser_tabs_model.h"
#include "chromeos/components/phonehub/phone_status_model.h"

namespace chromeos {
namespace phonehub {

extern const char kFakeMobileProviderName[];

extern const char kFakeBrowserTabUrl1[];
extern const char kFakeBrowserTabName1[];

extern const char kFakeBrowserTabUrl2[];
extern const char kFakeBrowserTabName2[];

// Creates fake phone status data for use in tests.
const PhoneStatusModel::MobileConnectionMetadata&
CreateFakeMobileConnectionMetadata();
const PhoneStatusModel& CreateFakePhoneStatusModel();

// Creates fake browser tab data for use in tests.
const BrowserTabsModel::BrowserTabMetadata& CreateFakeBrowserTabMetadata();
const BrowserTabsModel& CreateFakeBrowserTabsModel();

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_PHONE_MODEL_TEST_UTIL_H_
