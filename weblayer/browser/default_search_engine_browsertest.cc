// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/test/browser_test_utils.h"
#include "weblayer/browser/host_content_settings_map_factory.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {
namespace {
constexpr char kDseOrigin[] = "https://www.google.com";
}

using DefaultSearchEngineBrowserTest = WebLayerBrowserTest;

IN_PROC_BROWSER_TEST_F(DefaultSearchEngineBrowserTest,
                       HasGeolocationPermission) {
  auto* settings_map = HostContentSettingsMapFactory::GetForBrowserContext(
      static_cast<TabImpl*>(shell()->tab())
          ->web_contents()
          ->GetBrowserContext());
  EXPECT_EQ(settings_map->GetContentSetting(GURL(kDseOrigin), GURL(kDseOrigin),
                                            ContentSettingsType::GEOLOCATION,
                                            std::string()),
            CONTENT_SETTING_ALLOW);
  EXPECT_EQ(settings_map->GetContentSetting(GURL(kDseOrigin), GURL(kDseOrigin),
                                            ContentSettingsType::NOTIFICATIONS,
                                            std::string()),
            CONTENT_SETTING_ASK);
}

class IncognitoDefaultSearchEngineBrowserTest
    : public DefaultSearchEngineBrowserTest {
 public:
  IncognitoDefaultSearchEngineBrowserTest() { SetShellStartsInIncognitoMode(); }
};

IN_PROC_BROWSER_TEST_F(IncognitoDefaultSearchEngineBrowserTest,
                       IncognitoDoesNotHaveGeolocationPermission) {
  auto* settings_map = HostContentSettingsMapFactory::GetForBrowserContext(
      static_cast<TabImpl*>(shell()->tab())
          ->web_contents()
          ->GetBrowserContext());
  EXPECT_EQ(settings_map->GetContentSetting(GURL(kDseOrigin), GURL(kDseOrigin),
                                            ContentSettingsType::GEOLOCATION,
                                            std::string()),
            CONTENT_SETTING_ASK);
  EXPECT_EQ(settings_map->GetContentSetting(GURL(kDseOrigin), GURL(kDseOrigin),
                                            ContentSettingsType::NOTIFICATIONS,
                                            std::string()),
            CONTENT_SETTING_ASK);
}

}  // namespace weblayer
