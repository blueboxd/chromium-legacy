// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/search/search_handler.h"

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_localized_strings_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search.mojom-test-utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/services/local_search_service/local_search_service_impl.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace settings {

class SearchHandlerTest : public testing::Test {
 protected:
  SearchHandlerTest() : profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~SearchHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

    provider_ = std::make_unique<OsSettingsLocalizedStringsProvider>(
        profile_manager_.CreateTestingProfile("TestingProfile"),
        &local_search_service_);

    handler_ = std::make_unique<SearchHandler>(provider_.get(),
                                               &local_search_service_);
    handler_->BindInterface(handler_remote_.BindNewPipeAndPassReceiver());

    // Allow asynchronous networking code to complete; specifically, let code
    // which adds Wi-Fi to the system run so that Wi-Fi search tags can be
    // queried below.
    base::RunLoop().RunUntilIdle();
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  chromeos::network_config::CrosNetworkConfigTestHelper network_config_helper_;
  mojo::Remote<mojom::SearchHandler> handler_remote_;
  local_search_service::LocalSearchServiceImpl local_search_service_;
  std::unique_ptr<OsSettingsLocalizedStringsProvider> provider_;
  std::unique_ptr<SearchHandler> handler_;
};

TEST_F(SearchHandlerTest, Success) {
  // Search for "Wi-Fi".
  std::vector<mojom::SearchResultPtr> search_results;
  mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(base::ASCIIToUTF16("Wi-Fi"), &search_results);

  // Multiple results should be available, and they should have Wi-Fi metadata.
  EXPECT_GT(search_results.size(), 0u);
  for (const auto& result : search_results) {
    EXPECT_EQ(chrome::kWiFiSettingsSubPage, result->url_path_with_parameters);
    EXPECT_EQ(mojom::SearchResultIcon::kWifi, result->icon);
  }
}

TEST_F(SearchHandlerTest, NoResults) {
  std::vector<mojom::SearchResultPtr> search_results;
  mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(base::ASCIIToUTF16("QueryWithNoResults"), &search_results);
  EXPECT_TRUE(search_results.empty());
}

}  // namespace settings
}  // namespace chromeos
