// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/os_settings_localized_strings_provider.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_concept.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/services/local_search_service/local_search_service_impl.h"
#include "chrome/services/local_search_service/test_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace settings {

class OsSettingsLocalizedStringsProviderTest : public testing::Test {
 protected:
  OsSettingsLocalizedStringsProviderTest()
      : provider_(&local_search_service_) {}
  ~OsSettingsLocalizedStringsProviderTest() override = default;

  // testing::Test:
  void SetUp() override {
    local_search_service_.GetIndex(
        local_search_service::mojom::LocalSearchService::IndexId::CROS_SETTINGS,
        index_remote_.BindNewPipeAndPassReceiver());
  }

  base::test::TaskEnvironment task_environment_;
  mojo::Remote<local_search_service::mojom::Index> index_remote_;
  local_search_service::LocalSearchServiceImpl local_search_service_;
  OsSettingsLocalizedStringsProvider provider_;
};

// This tests the contents of Wi-Fi tags in particular by verifying data in
// os_settings_localized_strings_provider.cc. When Wi-Fi tags are updated, this
// test will need to be updated as well.
TEST_F(OsSettingsLocalizedStringsProviderTest, WifiTags) {
  provider_.AddWifiSearchTags();
  local_search_service::GetSizeAndCheck(index_remote_.get(), 5u);

  const SearchConcept* wifi_settings_concept =
      provider_.GetCanonicalTagMetadata(IDS_SETTINGS_TAG_WIFI_SETTINGS);
  ASSERT_TRUE(wifi_settings_concept);
  EXPECT_EQ(chrome::kWiFiSettingsSubPage,
            wifi_settings_concept->url_path_with_parameters);
  EXPECT_EQ(mojom::SearchResultIcon::kWifi, wifi_settings_concept->icon);

  provider_.RemoveWifiSearchTags();
  local_search_service::GetSizeAndCheck(index_remote_.get(), 0u);

  wifi_settings_concept =
      provider_.GetCanonicalTagMetadata(IDS_SETTINGS_TAG_WIFI_SETTINGS);
  EXPECT_FALSE(wifi_settings_concept);
}

// Note that other tests do not need to be added for different group of tags,
// since these tests would only be verifying the contents of
// os_settings_localized_strings_provider.cc.

}  // namespace settings
}  // namespace chromeos
