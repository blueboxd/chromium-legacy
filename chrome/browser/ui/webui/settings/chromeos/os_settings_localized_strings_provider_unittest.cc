// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/os_settings_localized_strings_provider.h"

#include "base/run_loop.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_concept.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/services/local_search_service/local_search_service_impl.h"
#include "chrome/services/local_search_service/public/mojom/local_search_service.mojom-test-utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/network/network_state_test_helper.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {
namespace settings {

class OsSettingsLocalizedStringsProviderTest : public testing::Test {
 protected:
  OsSettingsLocalizedStringsProviderTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~OsSettingsLocalizedStringsProviderTest() override = default;

  // testing::Test:
  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

    provider_ = std::make_unique<OsSettingsLocalizedStringsProvider>(
        profile_manager_.CreateTestingProfile("TestingProfile"),
        &local_search_service_);

    local_search_service_.GetIndex(
        local_search_service::mojom::LocalSearchService::IndexId::CROS_SETTINGS,
        index_remote_.BindNewPipeAndPassReceiver());

    // Allow asynchronous networking code to complete (networking functionality
    // is tested below).
    base::RunLoop().RunUntilIdle();
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  chromeos::network_config::CrosNetworkConfigTestHelper network_config_helper_;
  mojo::Remote<local_search_service::mojom::Index> index_remote_;
  local_search_service::LocalSearchServiceImpl local_search_service_;
  std::unique_ptr<OsSettingsLocalizedStringsProvider> provider_;
};

// To prevent this from becoming a change-detector test, this test simply
// verifies that when the provider starts up, it adds *some* strings without
// checking the exact number. It also checks one specific canonical tag.
TEST_F(OsSettingsLocalizedStringsProviderTest, WifiTags) {
  uint64_t initial_num_items = 0;
  local_search_service::mojom::IndexAsyncWaiter(index_remote_.get())
      .GetSize(&initial_num_items);
  EXPECT_GT(initial_num_items, 0u);

  const SearchConcept* network_settings_concept =
      provider_->GetCanonicalTagMetadata(IDS_SETTINGS_TAG_NETWORK_SETTINGS);
  ASSERT_TRUE(network_settings_concept);
  EXPECT_EQ(chrome::kNetworksSubPage,
            network_settings_concept->url_path_with_parameters);
  EXPECT_EQ(mojom::SearchResultIcon::kWifi, network_settings_concept->icon);

  // Ethernet is not present by default, so no Ethernet concepts have yet been
  // added.
  const SearchConcept* ethernet_settings_concept =
      provider_->GetCanonicalTagMetadata(IDS_SETTINGS_TAG_ETHERNET_SETTINGS);
  ASSERT_FALSE(ethernet_settings_concept);

  // Add Ethernet and let asynchronous handlers run. This should cause Ethernet
  // tags to be added.
  network_config_helper_.network_state_helper().device_test()->AddDevice(
      "/device/stub_eth_device", shill::kTypeEthernet, "stub_eth_device");
  base::RunLoop().RunUntilIdle();

  uint64_t num_items_after_adding_ethernet = 0;
  local_search_service::mojom::IndexAsyncWaiter(index_remote_.get())
      .GetSize(&num_items_after_adding_ethernet);
  EXPECT_GT(num_items_after_adding_ethernet, initial_num_items);

  ethernet_settings_concept =
      provider_->GetCanonicalTagMetadata(IDS_SETTINGS_TAG_ETHERNET_SETTINGS);
  ASSERT_TRUE(ethernet_settings_concept);
  EXPECT_EQ(chrome::kEthernetSettingsSubPage,
            ethernet_settings_concept->url_path_with_parameters);
  EXPECT_EQ(mojom::SearchResultIcon::kEthernet,
            ethernet_settings_concept->icon);
}

// Note that other tests do not need to be added for different group of tags,
// since these tests would only be verifying the contents of
// os_settings_localized_strings_provider.cc.

}  // namespace settings
}  // namespace chromeos
