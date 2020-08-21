// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/phone_model_test_util.h"

#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"

namespace chromeos {
namespace phonehub {

const char kFakeMobileProviderName[] = "Fake Mobile Provider";

const char kFakeBrowserTabUrl1[] = "https://www.example.com/tab1";
const char kFakeBrowserTabName1[] = "Tab 1";

const char kFakeBrowserTabUrl2[] = "https://www.example.com/tab2";
const char kFakeBrowserTabName2[] = "Tab 2";

const PhoneStatusModel::MobileConnectionMetadata&
CreateFakeMobileConnectionMetadata() {
  static const base::NoDestructor<PhoneStatusModel::MobileConnectionMetadata>
      fake_mobile_connection_metadata{
          {PhoneStatusModel::SignalStrength::kFourBars,
           base::UTF8ToUTF16(kFakeMobileProviderName)}};
  return *fake_mobile_connection_metadata;
}

const PhoneStatusModel& CreateFakePhoneStatusModel() {
  static const base::NoDestructor<PhoneStatusModel> fake_phone_status_model{
      PhoneStatusModel::MobileStatus::kSimWithReception,
      CreateFakeMobileConnectionMetadata(),
      PhoneStatusModel::ChargingState::kNotCharging,
      PhoneStatusModel::BatterySaverState::kOff,
      /*battery_percentage=*/100u};
  return *fake_phone_status_model;
}

const BrowserTabsModel::BrowserTabMetadata& CreateFakeBrowserTabMetadata() {
  static const base::NoDestructor<BrowserTabsModel::BrowserTabMetadata>
      fake_browser_tab_metadata{GURL(kFakeBrowserTabUrl1),
                                base::UTF8ToUTF16(kFakeBrowserTabName1),
                                base::Time(), gfx::Image()};
  return *fake_browser_tab_metadata;
}

const BrowserTabsModel& CreateFakeBrowserTabsModel() {
  static const base::NoDestructor<BrowserTabsModel::BrowserTabMetadata>
      second_browser_tab_metadata{GURL(kFakeBrowserTabUrl2),
                                  base::UTF8ToUTF16(kFakeBrowserTabName2),
                                  base::Time(), gfx::Image()};

  static const base::NoDestructor<BrowserTabsModel> fake_browser_tabs_model{
      /*is_tab_sync_enabled=*/true, CreateFakeBrowserTabMetadata(),
      *second_browser_tab_metadata};

  return *fake_browser_tabs_model;
}

}  // namespace phonehub
}  // namespace chromeos
