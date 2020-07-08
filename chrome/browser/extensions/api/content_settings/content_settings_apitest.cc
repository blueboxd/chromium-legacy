// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/api/content_settings/content_settings_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_result.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/scoped_worker_based_extensions_channel.h"

namespace extensions {

using ContextType = ExtensionApiTest::ContextType;

class ExtensionContentSettingsApiTest : public ExtensionApiTest {
 public:
  ExtensionContentSettingsApiTest() : profile_(nullptr) {}

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();

    // The browser might get closed later (and therefore be destroyed), so we
    // save the profile.
    profile_ = browser()->profile();

    // Closing the last browser window also releases a KeepAlive. Make
    // sure it's not the last one, so the message loop doesn't quit
    // unexpectedly.
    keep_alive_.reset(new ScopedKeepAlive(KeepAliveOrigin::BROWSER,
                                          KeepAliveRestartOption::DISABLED));
  }

  void TearDownOnMainThread() override {
    // BrowserProcess::Shutdown() needs to be called in a message loop, so we
    // post a task to release the keep alive, then run the message loop.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&std::unique_ptr<ScopedKeepAlive>::reset,
                                  base::Unretained(&keep_alive_), nullptr));
    content::RunAllPendingInMessageLoop();

    ExtensionApiTest::TearDownOnMainThread();
  }

 protected:
  void CheckContentSettingsSet() {
    HostContentSettingsMap* map =
        HostContentSettingsMapFactory::GetForProfile(profile_);
    content_settings::CookieSettings* cookie_settings =
        CookieSettingsFactory::GetForProfile(profile_).get();

    // Check default content settings by using an unknown URL.
    GURL example_url("http://www.example.com");
    EXPECT_TRUE(
        cookie_settings->IsCookieAccessAllowed(example_url, example_url));
    EXPECT_TRUE(cookie_settings->IsCookieSessionOnly(example_url));
    EXPECT_EQ(
        CONTENT_SETTING_ALLOW,
        map->GetContentSetting(example_url, example_url,
                               ContentSettingsType::IMAGES, std::string()));
    EXPECT_EQ(
        CONTENT_SETTING_BLOCK,
        map->GetContentSetting(example_url, example_url,
                               ContentSettingsType::JAVASCRIPT, std::string()));
    EXPECT_EQ(
        CONTENT_SETTING_BLOCK,
        map->GetContentSetting(example_url, example_url,
                               ContentSettingsType::PLUGINS, std::string()));
    EXPECT_EQ(
        CONTENT_SETTING_BLOCK,
        map->GetContentSetting(example_url, example_url,
                               ContentSettingsType::POPUPS, std::string()));
    EXPECT_EQ(CONTENT_SETTING_ASK,
              map->GetContentSetting(example_url, example_url,
                                     ContentSettingsType::GEOLOCATION,
                                     std::string()));
    EXPECT_EQ(CONTENT_SETTING_ASK,
              map->GetContentSetting(example_url, example_url,
                                     ContentSettingsType::NOTIFICATIONS,
                                     std::string()));
    EXPECT_EQ(CONTENT_SETTING_ASK,
              map->GetContentSetting(example_url, example_url,
                                     ContentSettingsType::MEDIASTREAM_MIC,
                                     std::string()));
    EXPECT_EQ(CONTENT_SETTING_ASK,
              map->GetContentSetting(example_url, example_url,
                                     ContentSettingsType::MEDIASTREAM_CAMERA,
                                     std::string()));
    EXPECT_EQ(CONTENT_SETTING_ASK,
              map->GetContentSetting(example_url, example_url,
                                     ContentSettingsType::PPAPI_BROKER,
                                     std::string()));
    EXPECT_EQ(CONTENT_SETTING_ASK,
              map->GetContentSetting(example_url, example_url,
                                     ContentSettingsType::AUTOMATIC_DOWNLOADS,
                                     std::string()));
    EXPECT_EQ(
        CONTENT_SETTING_ALLOW,
        map->GetContentSetting(example_url, example_url,
                               ContentSettingsType::AUTOPLAY, std::string()));

    // Check content settings for www.google.com
    GURL url("http://www.google.com");
    EXPECT_FALSE(cookie_settings->IsCookieAccessAllowed(url, url));
    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              map->GetContentSetting(url, url, ContentSettingsType::IMAGES,
                                     std::string()));
    EXPECT_EQ(CONTENT_SETTING_BLOCK,
              map->GetContentSetting(url, url, ContentSettingsType::JAVASCRIPT,
                                     std::string()));
    EXPECT_EQ(CONTENT_SETTING_BLOCK,
              map->GetContentSetting(url, url, ContentSettingsType::PLUGINS,
                                     std::string()));
    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              map->GetContentSetting(url, url, ContentSettingsType::POPUPS,
                                     std::string()));
    EXPECT_EQ(CONTENT_SETTING_BLOCK,
              map->GetContentSetting(url, url, ContentSettingsType::GEOLOCATION,
                                     std::string()));
    EXPECT_EQ(CONTENT_SETTING_BLOCK,
              map->GetContentSetting(
                  url, url, ContentSettingsType::NOTIFICATIONS, std::string()));
    EXPECT_EQ(
        CONTENT_SETTING_BLOCK,
        map->GetContentSetting(url, url, ContentSettingsType::MEDIASTREAM_MIC,
                               std::string()));
    EXPECT_EQ(
        CONTENT_SETTING_BLOCK,
        map->GetContentSetting(
            url, url, ContentSettingsType::MEDIASTREAM_CAMERA, std::string()));
    EXPECT_EQ(CONTENT_SETTING_BLOCK,
              map->GetContentSetting(
                  url, url, ContentSettingsType::PPAPI_BROKER, std::string()));
    EXPECT_EQ(
        CONTENT_SETTING_BLOCK,
        map->GetContentSetting(
            url, url, ContentSettingsType::AUTOMATIC_DOWNLOADS, std::string()));
    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              map->GetContentSetting(url, url, ContentSettingsType::AUTOPLAY,
                                     std::string()));
  }

  void CheckContentSettingsDefault() {
    HostContentSettingsMap* map =
        HostContentSettingsMapFactory::GetForProfile(profile_);
    content_settings::CookieSettings* cookie_settings =
        CookieSettingsFactory::GetForProfile(profile_).get();

    // Check content settings for www.google.com
    GURL url("http://www.google.com");
    EXPECT_TRUE(cookie_settings->IsCookieAccessAllowed(url, url));
    EXPECT_FALSE(cookie_settings->IsCookieSessionOnly(url));
    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              map->GetContentSetting(url, url, ContentSettingsType::IMAGES,
                                     std::string()));
    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              map->GetContentSetting(url, url, ContentSettingsType::JAVASCRIPT,
                                     std::string()));
    EXPECT_EQ(CONTENT_SETTING_BLOCK,
              map->GetContentSetting(url, url, ContentSettingsType::PLUGINS,
                                     std::string()));
    EXPECT_EQ(CONTENT_SETTING_BLOCK,
              map->GetContentSetting(url, url, ContentSettingsType::POPUPS,
                                     std::string()));
    EXPECT_EQ(CONTENT_SETTING_ASK,
              map->GetContentSetting(url, url, ContentSettingsType::GEOLOCATION,
                                     std::string()));
    EXPECT_EQ(CONTENT_SETTING_ASK,
              map->GetContentSetting(
                  url, url, ContentSettingsType::NOTIFICATIONS, std::string()));
    EXPECT_EQ(
        CONTENT_SETTING_ASK,
        map->GetContentSetting(url, url, ContentSettingsType::MEDIASTREAM_MIC,
                               std::string()));
    EXPECT_EQ(
        CONTENT_SETTING_ASK,
        map->GetContentSetting(
            url, url, ContentSettingsType::MEDIASTREAM_CAMERA, std::string()));
    EXPECT_EQ(CONTENT_SETTING_ASK,
              map->GetContentSetting(
                  url, url, ContentSettingsType::PPAPI_BROKER, std::string()));
    EXPECT_EQ(
        CONTENT_SETTING_ASK,
        map->GetContentSetting(
            url, url, ContentSettingsType::AUTOMATIC_DOWNLOADS, std::string()));
    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              map->GetContentSetting(url, url, ContentSettingsType::AUTOPLAY,
                                     std::string()));
  }

  // Returns a snapshot of content settings for a given URL.
  std::vector<int> GetContentSettingsSnapshot(const GURL& url) {
    std::vector<int> content_settings;

    HostContentSettingsMap* map =
        HostContentSettingsMapFactory::GetForProfile(profile_);
    content_settings::CookieSettings* cookie_settings =
        CookieSettingsFactory::GetForProfile(profile_).get();

    content_settings.push_back(
        cookie_settings->IsCookieAccessAllowed(url, url));
    content_settings.push_back(cookie_settings->IsCookieSessionOnly(url));
    content_settings.push_back(map->GetContentSetting(
        url, url, ContentSettingsType::IMAGES, std::string()));
    content_settings.push_back(map->GetContentSetting(
        url, url, ContentSettingsType::JAVASCRIPT, std::string()));
    content_settings.push_back(map->GetContentSetting(
        url, url, ContentSettingsType::PLUGINS, std::string()));
    content_settings.push_back(map->GetContentSetting(
        url, url, ContentSettingsType::POPUPS, std::string()));
    content_settings.push_back(map->GetContentSetting(
        url, url, ContentSettingsType::GEOLOCATION, std::string()));
    content_settings.push_back(map->GetContentSetting(
        url, url, ContentSettingsType::NOTIFICATIONS, std::string()));
    content_settings.push_back(map->GetContentSetting(
        url, url, ContentSettingsType::MEDIASTREAM_MIC, std::string()));
    content_settings.push_back(map->GetContentSetting(
        url, url, ContentSettingsType::MEDIASTREAM_CAMERA, std::string()));
    content_settings.push_back(map->GetContentSetting(
        url, url, ContentSettingsType::PPAPI_BROKER, std::string()));
    content_settings.push_back(map->GetContentSetting(
        url, url, ContentSettingsType::AUTOMATIC_DOWNLOADS, std::string()));
    content_settings.push_back(map->GetContentSetting(
        url, url, ContentSettingsType::AUTOPLAY, std::string()));
    return content_settings;
  }

 private:
  Profile* profile_;
  std::unique_ptr<ScopedKeepAlive> keep_alive_;
};

class ExtensionContentSettingsApiLazyTest
    : public ExtensionContentSettingsApiTest,
      public testing::WithParamInterface<ContextType> {
 public:
  void SetUp() override {
    ExtensionContentSettingsApiTest::SetUp();
    // Service Workers are currently only available on certain channels, so set
    // the channel for those tests.
    if (GetParam() == ContextType::kServiceWorker)
      current_channel_ = std::make_unique<ScopedWorkerBasedExtensionsChannel>();
  }

 protected:
  bool RunLazyTest(const std::string& extension_name) {
    return RunLazyTestWithArg(extension_name, nullptr);
  }

  bool RunLazyTestWithArg(const std::string& extension_name, const char* arg) {
    int browser_test_flags = kFlagNone;
    if (GetParam() == ContextType::kServiceWorker)
      browser_test_flags |= kFlagRunAsServiceWorkerBasedExtension;

    return RunExtensionTestWithFlagsAndArg(extension_name, arg,
                                           browser_test_flags, kFlagNone);
  }

  std::unique_ptr<ScopedWorkerBasedExtensionsChannel> current_channel_;
};

INSTANTIATE_TEST_SUITE_P(EventPage,
                         ExtensionContentSettingsApiLazyTest,
                         ::testing::Values(ContextType::kEventPage));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         ExtensionContentSettingsApiLazyTest,
                         ::testing::Values(ContextType::kServiceWorker));

IN_PROC_BROWSER_TEST_F(ExtensionContentSettingsApiTest, Standard) {
  CheckContentSettingsDefault();

  const char kExtensionPath[] = "content_settings/standard";

  EXPECT_TRUE(RunExtensionSubtest(kExtensionPath, "test.html")) << message_;
  CheckContentSettingsSet();

  // The settings should not be reset when the extension is reloaded.
  ReloadExtension(last_loaded_extension_id());
  CheckContentSettingsSet();

  // Uninstalling and installing the extension (without running the test that
  // calls the extension API) should clear the settings.
  TestExtensionRegistryObserver observer(ExtensionRegistry::Get(profile()),
                                         last_loaded_extension_id());
  UninstallExtension(last_loaded_extension_id());
  observer.WaitForExtensionUninstalled();
  CheckContentSettingsDefault();

  LoadExtension(test_data_dir_.AppendASCII(kExtensionPath));
  CheckContentSettingsDefault();
}

// TODO(crbug.com/1073588): Make this test work in branded builds.
// Pass the plugins to look for into the JS to make this test less
// brittle or just have the JS side look for the additional plugins.
//
// Flaky on the trybots. See http://crbug.com/96725.
IN_PROC_BROWSER_TEST_P(ExtensionContentSettingsApiLazyTest,
                       DISABLED_GetResourceIdentifiers) {
  base::FilePath::CharType kFooPath[] =
      FILE_PATH_LITERAL("/plugins/foo.plugin");
  base::FilePath::CharType kBarPath[] =
      FILE_PATH_LITERAL("/plugins/bar.plugin");
  const char kFooName[] = "Foo Plugin";
  const char kBarName[] = "Bar Plugin";

  content::PluginService::GetInstance()->RegisterInternalPlugin(
      content::WebPluginInfo(base::ASCIIToUTF16(kFooName),
                             base::FilePath(kFooPath),
                             base::ASCIIToUTF16("1.2.3"),
                             base::ASCIIToUTF16("foo")),
      false);
  content::PluginService::GetInstance()->RegisterInternalPlugin(
    content::WebPluginInfo(base::ASCIIToUTF16(kBarName),
                           base::FilePath(kBarPath),
                           base::ASCIIToUTF16("2.3.4"),
                           base::ASCIIToUTF16("bar")),
      false);

  EXPECT_TRUE(RunLazyTest("content_settings/getresourceidentifiers"))
      << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionContentSettingsApiLazyTest,
                       UnsupportedDefaultSettings) {
  const char kExtensionPath[] = "content_settings/unsupporteddefaultsettings";
  EXPECT_TRUE(RunExtensionTest(kExtensionPath)) << message_;
}

// Tests if an extension clearing content settings for one content type leaves
// the others unchanged.
IN_PROC_BROWSER_TEST_P(ExtensionContentSettingsApiLazyTest,
                       ClearProperlyGranular) {
  const char kExtensionPath[] = "content_settings/clearproperlygranular";
  EXPECT_TRUE(RunLazyTest(kExtensionPath)) << message_;
}

// Tests if changing permissions in incognito mode keeps the previous state of
// regular mode.
IN_PROC_BROWSER_TEST_F(ExtensionContentSettingsApiTest, IncognitoIsolation) {
  GURL url("http://www.example.com");

  // Record previous state of content settings.
  std::vector<int> content_settings_before = GetContentSettingsSnapshot(url);

  // Run extension, set all permissions to allow, and check if they are changed.
  EXPECT_TRUE(RunExtensionSubtestWithArgAndFlags(
      "content_settings/incognitoisolation", "test.html", "allow",
      kFlagEnableIncognito, kFlagUseIncognito))
      << message_;

  // Get content settings after running extension to ensure nothing is changed.
  std::vector<int> content_settings_after = GetContentSettingsSnapshot(url);
  EXPECT_EQ(content_settings_before, content_settings_after);

  // Run extension, set all permissions to block, and check if they are changed.
  EXPECT_TRUE(RunExtensionSubtestWithArgAndFlags(
      "content_settings/incognitoisolation", "test.html", "block",
      kFlagEnableIncognito, kFlagUseIncognito))
      << message_;

  // Get content settings after running extension to ensure nothing is changed.
  content_settings_after = GetContentSettingsSnapshot(url);
  EXPECT_EQ(content_settings_before, content_settings_after);
}

// Tests if changing incognito mode permissions in regular profile are rejected.
IN_PROC_BROWSER_TEST_F(ExtensionContentSettingsApiTest,
                       IncognitoNotAllowedInRegular) {
  EXPECT_FALSE(RunExtensionSubtestWithArg("content_settings/incognitoisolation",
                                          "test.html", "allow"))
      << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionContentSettingsApiLazyTest,
                       EmbeddedSettingsMetric) {
  base::HistogramTester histogram_tester;
  const char kExtensionPath[] = "content_settings/embeddedsettingsmetric";
  EXPECT_TRUE(RunLazyTest(kExtensionPath)) << message_;

  size_t num_values = 0;
  int images_type = ContentSettingTypeToHistogramValue(
      ContentSettingsType::IMAGES, &num_values);
  int geolocation_type = ContentSettingTypeToHistogramValue(
      ContentSettingsType::GEOLOCATION, &num_values);
  int cookies_type = ContentSettingTypeToHistogramValue(
      ContentSettingsType::COOKIES, &num_values);

  histogram_tester.ExpectBucketCount(
      "ContentSettings.ExtensionEmbeddedSettingSet", images_type, 1);
  histogram_tester.ExpectBucketCount(
      "ContentSettings.ExtensionEmbeddedSettingSet", geolocation_type, 1);
  histogram_tester.ExpectTotalCount(
      "ContentSettings.ExtensionEmbeddedSettingSet", 2);

  histogram_tester.ExpectBucketCount(
      "ContentSettings.ExtensionNonEmbeddedSettingSet", images_type, 1);
  histogram_tester.ExpectBucketCount(
      "ContentSettings.ExtensionNonEmbeddedSettingSet", cookies_type, 1);
  histogram_tester.ExpectTotalCount(
      "ContentSettings.ExtensionNonEmbeddedSettingSet", 2);
}

class ExtensionContentSettingsApiTestWithPermissionDelegationDisabled
    : public ExtensionContentSettingsApiLazyTest {
 public:
  ExtensionContentSettingsApiTestWithPermissionDelegationDisabled() {
    feature_list_.InitAndDisableFeature(
        permissions::features::kPermissionDelegation);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    EventPage,
    ExtensionContentSettingsApiTestWithPermissionDelegationDisabled,
    ::testing::Values(ContextType::kEventPage));

INSTANTIATE_TEST_SUITE_P(
    ServiceWorker,
    ExtensionContentSettingsApiTestWithPermissionDelegationDisabled,
    ::testing::Values(ContextType::kServiceWorker));

class ExtensionContentSettingsApiTestWithPermissionDelegationEnabled
    : public ExtensionContentSettingsApiLazyTest {
 public:
  ExtensionContentSettingsApiTestWithPermissionDelegationEnabled() {
    feature_list_.InitAndEnableFeature(
        permissions::features::kPermissionDelegation);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    EventPage,
    ExtensionContentSettingsApiTestWithPermissionDelegationEnabled,
    ::testing::Values(ContextType::kEventPage));

INSTANTIATE_TEST_SUITE_P(
    ServiceWorker,
    ExtensionContentSettingsApiTestWithPermissionDelegationEnabled,
    ::testing::Values(ContextType::kServiceWorker));

IN_PROC_BROWSER_TEST_P(
    ExtensionContentSettingsApiTestWithPermissionDelegationDisabled,
    EmbeddedSettings) {
  const char kExtensionPath[] = "content_settings/embeddedsettings";
  EXPECT_TRUE(RunLazyTestWithArg(kExtensionPath, nullptr)) << message_;
}

IN_PROC_BROWSER_TEST_P(
    ExtensionContentSettingsApiTestWithPermissionDelegationEnabled,
    EmbeddedSettings) {
  const char kExtensionPath[] = "content_settings/embeddedsettings";
  EXPECT_TRUE(RunLazyTestWithArg(kExtensionPath, "permission")) << message_;
}

class ExtensionContentSettingsApiTestWithWildcardMatchingDisabled
    : public ExtensionContentSettingsApiLazyTest {
 public:
  ExtensionContentSettingsApiTestWithWildcardMatchingDisabled() {
    scoped_feature_list_.InitAndEnableFeature(
        content_settings::kDisallowWildcardsInPluginContentSettings);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    EventPage,
    ExtensionContentSettingsApiTestWithWildcardMatchingDisabled,
    ::testing::Values(ContextType::kEventPage));

INSTANTIATE_TEST_SUITE_P(
    ServiceWorker,
    ExtensionContentSettingsApiTestWithWildcardMatchingDisabled,
    ::testing::Values(ContextType::kServiceWorker));

IN_PROC_BROWSER_TEST_P(
    ExtensionContentSettingsApiTestWithWildcardMatchingDisabled,
    PluginTest) {
  constexpr char kExtensionPath[] = "content_settings/pluginswildcardmatching";
  EXPECT_TRUE(RunLazyTest(kExtensionPath)) << message_;

  constexpr char kGoogleMailUrl[] = "http://mail.google.com:443";
  constexpr char kGoogleDriveUrl[] = "http://drive.google.com:443";

  permissions::PermissionManager* permission_manager =
      PermissionManagerFactory::GetForProfile(browser()->profile());
  EXPECT_EQ(
      permission_manager
          ->GetPermissionStatus(ContentSettingsType::PLUGINS,
                                GURL(kGoogleMailUrl), GURL(kGoogleMailUrl))
          .content_setting,
      ContentSetting::CONTENT_SETTING_BLOCK);

  EXPECT_EQ(
      permission_manager
          ->GetPermissionStatus(ContentSettingsType::PLUGINS,
                                GURL(kGoogleDriveUrl), GURL(kGoogleDriveUrl))
          .content_setting,
      ContentSetting::CONTENT_SETTING_ALLOW);
}

}  // namespace extensions
