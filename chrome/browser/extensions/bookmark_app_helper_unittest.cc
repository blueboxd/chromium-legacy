// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/bookmark_app_helper.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/extensions/convert_web_app.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/installable/installable_data.h"
#include "chrome/browser/web_applications/components/web_app_icon_downloader.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/extensions/manifest_handlers/app_theme_color_info.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/skia_util.h"

namespace extensions {

namespace {

using ForInstallableSite = web_app::ForInstallableSite;

const char kManifestUrl[] = "http://www.chromium.org/manifest.json";
const char kAppUrl[] = "http://www.chromium.org/index.html";
const char kAppScope[] = "http://www.chromium.org/scope/";
const char kAppAlternativeScope[] = "http://www.chromium.org/new/";
const char kAppDefaultScope[] = "http://www.chromium.org/";
const char kAppTitle[] = "Test title";
const char kAlternativeAppTitle[] = "Different test title";
const char kAppDescription[] = "Test description";
const char kAppIconURL1[] = "http://foo.com/1.png";
const char kAppIconURL2[] = "http://foo.com/2.png";

const int kIconSizeTiny = extension_misc::EXTENSION_ICON_BITTY;
const int kIconSizeSmall = extension_misc::EXTENSION_ICON_SMALL;
const int kIconSizeMedium = extension_misc::EXTENSION_ICON_MEDIUM;
const int kIconSizeLarge = extension_misc::EXTENSION_ICON_LARGE;

class BookmarkAppHelperTest : public testing::Test {
 public:
  BookmarkAppHelperTest() {}
  ~BookmarkAppHelperTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarkAppHelperTest);
};

class BookmarkAppHelperExtensionServiceTest
    : public extensions::ExtensionServiceTestBase {
 public:
  BookmarkAppHelperExtensionServiceTest() {}
  ~BookmarkAppHelperExtensionServiceTest() override {}

  void SetUp() override {
    extensions::ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();
    service_->Init();
    EXPECT_EQ(0u, registry()->enabled_extensions().size());
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  }

  void TearDown() override {
    ExtensionServiceTestBase::TearDown();
    for (content::RenderProcessHost::iterator i(
             content::RenderProcessHost::AllHostsIterator());
         !i.IsAtEnd();
         i.Advance()) {
      content::RenderProcessHost* host = i.GetCurrentValue();
      if (Profile::FromBrowserContext(host->GetBrowserContext()) ==
          profile_.get())
        host->Cleanup();
    }
  }

  content::WebContents* web_contents() { return web_contents_.get(); }

 private:
  std::unique_ptr<content::WebContents> web_contents_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkAppHelperExtensionServiceTest);
};

SkBitmap CreateSquareBitmapWithColor(int size, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size, size);
  bitmap.eraseColor(color);
  return bitmap;
}

WebApplicationInfo::IconInfo CreateIconInfoWithBitmap(int size, SkColor color) {
  WebApplicationInfo::IconInfo icon_info;
  icon_info.width = size;
  icon_info.height = size;
  icon_info.data = CreateSquareBitmapWithColor(size, color);
  return icon_info;
}

class TestBookmarkAppHelper : public BookmarkAppHelper {
 public:
  TestBookmarkAppHelper(ExtensionService* service,
                        const WebApplicationInfo& web_app_info,
                        content::WebContents* contents)
      : BookmarkAppHelper(service->profile(),
                          std::make_unique<WebApplicationInfo>(web_app_info),
                          contents,
                          WebappInstallSource::MENU_BROWSER_TAB),
        bitmap_(CreateSquareBitmapWithColor(32, SK_ColorRED)) {}

  ~TestBookmarkAppHelper() override {}

  void CreationComplete(const extensions::Extension* extension,
                        const WebApplicationInfo& web_app_info) {
    extension_ = extension;
  }

  void CompleteInstallableCheck(const char* manifest_url_str,
                                const blink::Manifest& manifest,
                                ForInstallableSite for_installable_site) {
    bool installable = for_installable_site == ForInstallableSite::kYes;
    GURL manifest_url(manifest_url_str);
    GURL primary_icon_url(kAppIconURL1);
    InstallableData data = {
        installable
            ? std::vector<InstallableStatusCode>()
            : std::vector<
                  InstallableStatusCode>{MANIFEST_DISPLAY_NOT_SUPPORTED},
        manifest_url,
        &manifest,
        primary_icon_url,
        &bitmap_,
        false,
        GURL::EmptyGURL(),
        nullptr,
        installable,
        installable,
    };
    BookmarkAppHelper::OnDidPerformInstallableCheck(data);
  }

  void CompleteIconDownload(
      bool success,
      const std::map<GURL, std::vector<SkBitmap>>& bitmaps) {
    BookmarkAppHelper::OnIconsDownloaded(success, bitmaps);
  }

  void FinishCreationForNonInstallableSite() {
    CompleteInstallableCheck("", blink::Manifest(), ForInstallableSite::kNo);

    std::map<GURL, std::vector<SkBitmap>> icon_map;
    icon_map[GURL(kAppUrl)].push_back(
        CreateSquareBitmapWithColor(kIconSizeSmall, SK_ColorRED));
    CompleteIconDownload(true, icon_map);

    content::RunAllTasksUntilIdle();
  }

  const Extension* extension() { return extension_; }

  const web_app::WebAppIconDownloader* web_app_icon_downloader() {
    return web_app_icon_downloader_.get();
  }

 private:
  const Extension* extension_;
  SkBitmap bitmap_;

  DISALLOW_COPY_AND_ASSIGN(TestBookmarkAppHelper);
};

}  // namespace

// Deprecated in favour of InstallManagerBookmarkAppTest.CreateBookmarkApp.
// TODO(crbug.com/915043): Erase it.
TEST_F(BookmarkAppHelperExtensionServiceTest, CreateBookmarkApp) {
  WebApplicationInfo web_app_info;
  web_app_info.app_url = GURL(kAppUrl);
  web_app_info.title = base::UTF8ToUTF16(kAppTitle);
  web_app_info.description = base::UTF8ToUTF16(kAppDescription);

  TestBookmarkAppHelper helper(service_, web_app_info, web_contents());
  helper.Create(base::Bind(&TestBookmarkAppHelper::CreationComplete,
                           base::Unretained(&helper)));
  helper.FinishCreationForNonInstallableSite();

  EXPECT_TRUE(helper.extension());
  const Extension* extension =
      service_->GetInstalledExtension(helper.extension()->id());
  EXPECT_TRUE(extension);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(extension->from_bookmark());
  EXPECT_FALSE(extension->was_installed_by_default());
  EXPECT_FALSE(Manifest::IsPolicyLocation(helper.extension()->location()));

  EXPECT_EQ(kAppTitle, extension->name());
  EXPECT_EQ(kAppDescription, extension->description());
  EXPECT_EQ(GURL(kAppUrl), AppLaunchInfo::GetLaunchWebURL(extension));
  EXPECT_FALSE(
      IconsInfo::GetIconResource(
          extension, kIconSizeSmall, ExtensionIconSet::MATCH_EXACTLY).empty());
  EXPECT_FALSE(
      AppBannerSettingsHelper::GetSingleBannerEvent(
          web_contents(), web_app_info.app_url, web_app_info.app_url.spec(),
          AppBannerSettingsHelper::APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN)
          .is_null());
}

// Deprecated in favour of
// InstallManagerBookmarkAppTest.CreateBookmarkAppDefaultApp.
// TODO(crbug.com/915043): Erase it.
TEST_F(BookmarkAppHelperExtensionServiceTest, CreateBookmarkAppDefaultApp) {
  WebApplicationInfo web_app_info;
  web_app_info.app_url = GURL(kAppUrl);
  web_app_info.title = base::UTF8ToUTF16(kAppTitle);
  web_app_info.description = base::UTF8ToUTF16(kAppDescription);

  TestBookmarkAppHelper helper(service_, web_app_info, web_contents());
  helper.set_is_default_app();
  helper.Create(base::Bind(&TestBookmarkAppHelper::CreationComplete,
                           base::Unretained(&helper)));
  helper.FinishCreationForNonInstallableSite();

  ASSERT_TRUE(helper.extension());
  EXPECT_TRUE(helper.extension()->from_bookmark());
  EXPECT_TRUE(helper.extension()->was_installed_by_default());
  EXPECT_EQ(Manifest::EXTERNAL_PREF_DOWNLOAD, helper.extension()->location());
  EXPECT_FALSE(Manifest::IsPolicyLocation(helper.extension()->location()));
}

// Deprecated in favour of
// InstallManagerBookmarkAppTest.CreateBookmarkAppPolicyInstalled.
// TODO(crbug.com/915043): Erase it.
TEST_F(BookmarkAppHelperExtensionServiceTest,
       CreateBookmarkAppPolicyInstalled) {
  WebApplicationInfo web_app_info;
  web_app_info.app_url = GURL(kAppUrl);
  web_app_info.title = base::UTF8ToUTF16(kAppTitle);
  web_app_info.description = base::UTF8ToUTF16(kAppDescription);

  TestBookmarkAppHelper helper(service_, web_app_info, web_contents());
  helper.set_is_policy_installed_app();
  helper.Create(base::Bind(&TestBookmarkAppHelper::CreationComplete,
                           base::Unretained(&helper)));
  helper.FinishCreationForNonInstallableSite();

  ASSERT_TRUE(helper.extension());
  EXPECT_TRUE(helper.extension()->from_bookmark());
  EXPECT_FALSE(helper.extension()->was_installed_by_default());
  EXPECT_TRUE(Manifest::IsPolicyLocation(helper.extension()->location()));
}

class BookmarkAppHelperExtensionServiceInstallableSiteTest
    : public BookmarkAppHelperExtensionServiceTest,
      public ::testing::WithParamInterface<ForInstallableSite> {
 public:
  BookmarkAppHelperExtensionServiceInstallableSiteTest() {}
  ~BookmarkAppHelperExtensionServiceInstallableSiteTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(
      BookmarkAppHelperExtensionServiceInstallableSiteTest);
};

// Deprecated in favour of
// InstallManagerBookmarkAppInstallableSiteTest.CreateBookmarkAppWithManifest.
// TODO(crbug.com/915043): Erase it.
TEST_P(BookmarkAppHelperExtensionServiceInstallableSiteTest,
       CreateBookmarkAppWithManifest) {
  WebApplicationInfo web_app_info;

  TestBookmarkAppHelper helper(service_, web_app_info, web_contents());
  helper.Create(base::Bind(&TestBookmarkAppHelper::CreationComplete,
                           base::Unretained(&helper)));

  blink::Manifest manifest;
  manifest.start_url = GURL(kAppUrl);
  manifest.name = base::NullableString16(base::UTF8ToUTF16(kAppTitle), false);
  manifest.scope = GURL(kAppScope);
  manifest.theme_color = SK_ColorBLUE;
  helper.CompleteInstallableCheck(kManifestUrl, manifest, GetParam());

  std::map<GURL, std::vector<SkBitmap>> icon_map;
  helper.CompleteIconDownload(true, icon_map);

  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(helper.extension());
  const Extension* extension =
      service_->GetInstalledExtension(helper.extension()->id());
  EXPECT_TRUE(extension);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(extension->from_bookmark());
  EXPECT_EQ(kAppTitle, extension->name());
  EXPECT_EQ(GURL(kAppUrl), AppLaunchInfo::GetLaunchWebURL(extension));
  EXPECT_EQ(SK_ColorBLUE, AppThemeColorInfo::GetThemeColor(extension).value());
  EXPECT_FALSE(
      AppBannerSettingsHelper::GetSingleBannerEvent(
          web_contents(), manifest.start_url, manifest.start_url.spec(),
          AppBannerSettingsHelper::APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN)
          .is_null());

  if (GetParam() == ForInstallableSite::kYes) {
    EXPECT_EQ(GURL(kAppScope), GetScopeURLFromBookmarkApp(extension));
  } else {
    EXPECT_EQ(GURL(), GetScopeURLFromBookmarkApp(extension));
  }
}

// Deprecated in favour of
// InstallManagerBookmarkAppInstallableSiteTest
// .CreateBookmarkAppWithManifestIcons.
// TODO(crbug.com/915043): Erase it.
TEST_P(BookmarkAppHelperExtensionServiceInstallableSiteTest,
       CreateBookmarkAppWithManifestIcons) {
  WebApplicationInfo web_app_info;
  TestBookmarkAppHelper helper(service_, web_app_info, web_contents());
  helper.Create(base::Bind(&TestBookmarkAppHelper::CreationComplete,
                           base::Unretained(&helper)));

  blink::Manifest manifest;
  manifest.start_url = GURL(kAppUrl);
  manifest.name = base::NullableString16(base::UTF8ToUTF16(kAppTitle), false);
  manifest.scope = GURL(kAppScope);
  blink::Manifest::ImageResource icon;
  icon.src = GURL(kAppIconURL1);
  icon.purpose = {blink::Manifest::ImageResource::Purpose::ANY};
  manifest.icons.push_back(icon);
  icon.src = GURL(kAppIconURL2);
  manifest.icons.push_back(icon);
  helper.CompleteInstallableCheck(kManifestUrl, manifest, GetParam());

  // Favicon URLs are ignored because the site has a manifest with icons.
  EXPECT_FALSE(helper.web_app_icon_downloader()->need_favicon_urls_);

  // Only 1 icon should be downloading since the other was provided by the
  // InstallableManager.
  EXPECT_EQ(1u, helper.web_app_icon_downloader()->in_progress_requests_.size());

  std::map<GURL, std::vector<SkBitmap>> icon_map;
  icon_map[GURL(kAppIconURL2)].push_back(
      CreateSquareBitmapWithColor(kIconSizeSmall, SK_ColorRED));
  helper.CompleteIconDownload(true, icon_map);

  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(helper.extension());
  const Extension* extension =
      service_->GetInstalledExtension(helper.extension()->id());
  EXPECT_TRUE(extension);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(extension->from_bookmark());
  EXPECT_EQ(kAppTitle, extension->name());
  EXPECT_EQ(GURL(kAppUrl), AppLaunchInfo::GetLaunchWebURL(extension));

  if (GetParam() == ForInstallableSite::kYes) {
    EXPECT_EQ(GURL(kAppScope), GetScopeURLFromBookmarkApp(extension));
  } else {
    EXPECT_EQ(GURL(), GetScopeURLFromBookmarkApp(extension));
  }
}

// Deprecated in favour of
// InstallManagerBookmarkAppInstallableSiteTest
// .CreateBookmarkAppWithManifestNoScope.
// TODO(crbug.com/915043): Erase it.
TEST_P(BookmarkAppHelperExtensionServiceInstallableSiteTest,
       CreateBookmarkAppWithManifestNoScope) {
  WebApplicationInfo web_app_info;

  TestBookmarkAppHelper helper(service_, web_app_info, web_contents());
  helper.Create(base::Bind(&TestBookmarkAppHelper::CreationComplete,
                           base::Unretained(&helper)));

  blink::Manifest manifest;
  manifest.start_url = GURL(kAppUrl);
  manifest.scope = GURL(kAppDefaultScope);
  manifest.name = base::NullableString16(base::UTF8ToUTF16(kAppTitle), false);
  helper.CompleteInstallableCheck(kManifestUrl, manifest, GetParam());

  std::map<GURL, std::vector<SkBitmap>> icon_map;
  helper.CompleteIconDownload(true, icon_map);
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(helper.extension());

  const Extension* extension =
      service_->GetInstalledExtension(helper.extension()->id());
  if (GetParam() == ForInstallableSite::kYes) {
    EXPECT_EQ(GURL(kAppDefaultScope), GetScopeURLFromBookmarkApp(extension));
  } else {
    EXPECT_EQ(GURL(), GetScopeURLFromBookmarkApp(extension));
  }
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         BookmarkAppHelperExtensionServiceInstallableSiteTest,
                         ::testing::Values(ForInstallableSite::kNo,
                                           ForInstallableSite::kYes));

// Deprecated in favour of
// InstallManagerBookmarkAppTest.CreateBookmarkAppDefaultLauncherContainers.
// TODO(crbug.com/915043): Erase it.
TEST_F(BookmarkAppHelperExtensionServiceTest,
       CreateBookmarkAppDefaultLauncherContainers) {
  std::map<GURL, std::vector<SkBitmap>> icon_map;

  blink::Manifest manifest;
  manifest.start_url = GURL(kAppUrl);
  manifest.name = base::NullableString16(base::UTF8ToUTF16(kAppTitle), false);
  manifest.scope = GURL(kAppScope);
  {
    WebApplicationInfo web_app_info;
    web_app_info.open_as_window = true;
    TestBookmarkAppHelper helper(service_, web_app_info, web_contents());
    helper.Create(base::Bind(&TestBookmarkAppHelper::CreationComplete,
                             base::Unretained(&helper)));

    helper.CompleteInstallableCheck(kManifestUrl, manifest,
                                    ForInstallableSite::kYes);
    helper.CompleteIconDownload(true, icon_map);

    content::RunAllTasksUntilIdle();
    EXPECT_TRUE(helper.extension());
    const Extension* extension =
        service_->GetInstalledExtension(helper.extension()->id());
    EXPECT_TRUE(extension);
    EXPECT_EQ(LaunchContainer::kLaunchContainerWindow,
              GetLaunchContainer(ExtensionPrefs::Get(profile()), extension));

    // Mark the app as not locally installed and check that it now opens in a
    // tab.
    SetBookmarkAppIsLocallyInstalled(profile(), extension, false);
    EXPECT_EQ(LaunchContainer::kLaunchContainerTab,
              GetLaunchContainer(ExtensionPrefs::Get(profile()), extension));

    // Mark the app as locally installed and check that it now opens in a
    // window.
    SetBookmarkAppIsLocallyInstalled(profile(), extension, true);
    EXPECT_EQ(LaunchContainer::kLaunchContainerWindow,
              GetLaunchContainer(ExtensionPrefs::Get(profile()), extension));
  }
  {
    WebApplicationInfo web_app_info;
    web_app_info.open_as_window = false;
    TestBookmarkAppHelper helper(service_, web_app_info, web_contents());
    helper.Create(base::Bind(&TestBookmarkAppHelper::CreationComplete,
                             base::Unretained(&helper)));

    helper.CompleteInstallableCheck(kManifestUrl, manifest,
                                    ForInstallableSite::kNo);
    helper.CompleteIconDownload(true, icon_map);

    content::RunAllTasksUntilIdle();
    EXPECT_TRUE(helper.extension());
    const Extension* extension =
        service_->GetInstalledExtension(helper.extension()->id());
    EXPECT_TRUE(extension);
    EXPECT_EQ(LaunchContainer::kLaunchContainerTab,
              GetLaunchContainer(ExtensionPrefs::Get(profile()), extension));
  }
}

// Deprecated in favour of
// InstallManagerBookmarkAppTest.CreateBookmarkAppForcedLauncherContainers.
// TODO(crbug.com/915043): Erase it.
TEST_F(BookmarkAppHelperExtensionServiceTest,
       CreateBookmarkAppForcedLauncherContainers) {
  WebApplicationInfo web_app_info;
  std::map<GURL, std::vector<SkBitmap>> icon_map;

  blink::Manifest manifest;
  manifest.start_url = GURL(kAppUrl);
  manifest.name = base::NullableString16(base::UTF8ToUTF16(kAppTitle), false);
  manifest.scope = GURL(kAppScope);
  {
    TestBookmarkAppHelper helper(service_, web_app_info, web_contents());
    helper.set_forced_launch_type(LAUNCH_TYPE_REGULAR);
    helper.Create(base::Bind(&TestBookmarkAppHelper::CreationComplete,
                             base::Unretained(&helper)));

    helper.CompleteInstallableCheck(kManifestUrl, manifest,
                                    ForInstallableSite::kYes);
    helper.CompleteIconDownload(true, icon_map);

    content::RunAllTasksUntilIdle();
    ASSERT_TRUE(helper.extension());
    EXPECT_EQ(
        LaunchContainer::kLaunchContainerTab,
        GetLaunchContainer(ExtensionPrefs::Get(profile()), helper.extension()));
  }
  {
    TestBookmarkAppHelper helper(service_, web_app_info, web_contents());
    helper.set_forced_launch_type(LAUNCH_TYPE_WINDOW);
    helper.Create(base::Bind(&TestBookmarkAppHelper::CreationComplete,
                             base::Unretained(&helper)));

    helper.CompleteInstallableCheck(kManifestUrl, manifest,
                                    ForInstallableSite::kNo);
    helper.CompleteIconDownload(true, icon_map);

    content::RunAllTasksUntilIdle();
    ASSERT_TRUE(helper.extension());
    EXPECT_EQ(
        LaunchContainer::kLaunchContainerWindow,
        GetLaunchContainer(ExtensionPrefs::Get(profile()), helper.extension()));
  }
}

// Deprecated in favour of
// InstallManagerBookmarkAppTest.CreateBookmarkAppWithoutManifest.
// TODO(crbug.com/915043): Erase it.
TEST_F(BookmarkAppHelperExtensionServiceTest,
       CreateBookmarkAppWithoutManifest) {
  WebApplicationInfo web_app_info;
  web_app_info.title = base::UTF8ToUTF16(kAppTitle);
  web_app_info.description = base::UTF8ToUTF16(kAppDescription);
  web_app_info.app_url = GURL(kAppUrl);

  TestBookmarkAppHelper helper(service_, web_app_info, web_contents());
  helper.Create(base::Bind(&TestBookmarkAppHelper::CreationComplete,
                           base::Unretained(&helper)));

  helper.CompleteInstallableCheck(kManifestUrl, blink::Manifest(),
                                  ForInstallableSite::kNo);
  std::map<GURL, std::vector<SkBitmap>> icon_map;
  helper.CompleteIconDownload(true, icon_map);
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(helper.extension());

  const Extension* extension =
      service_->GetInstalledExtension(helper.extension()->id());
  EXPECT_TRUE(extension);
  EXPECT_EQ(kAppTitle, extension->name());
  EXPECT_EQ(kAppDescription, extension->description());
  EXPECT_EQ(GURL(kAppUrl), AppLaunchInfo::GetLaunchWebURL(extension));
  EXPECT_EQ(GURL(), GetScopeURLFromBookmarkApp(extension));
  EXPECT_FALSE(AppThemeColorInfo::GetThemeColor(extension));
}

// Deprecated in favour of InstallManagerBookmarkAppTest.CreateWebAppFromInfo.
// TODO(crbug.com/915043): Erase it.
TEST_F(BookmarkAppHelperExtensionServiceTest, CreateBookmarkAppNoContents) {
  WebApplicationInfo web_app_info;
  web_app_info.app_url = GURL(kAppUrl);
  web_app_info.title = base::UTF8ToUTF16(kAppTitle);
  web_app_info.description = base::UTF8ToUTF16(kAppDescription);
  web_app_info.scope = GURL(kAppScope);
  web_app_info.icons.push_back(
      CreateIconInfoWithBitmap(kIconSizeTiny, SK_ColorRED));

  TestBookmarkAppHelper helper(service_, web_app_info, NULL);
  helper.Create(base::Bind(&TestBookmarkAppHelper::CreationComplete,
                           base::Unretained(&helper)));

  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(helper.extension());
  const Extension* extension =
      service_->GetInstalledExtension(helper.extension()->id());
  EXPECT_TRUE(extension);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(extension->from_bookmark());
  EXPECT_EQ(kAppTitle, extension->name());
  EXPECT_EQ(kAppDescription, extension->description());
  EXPECT_EQ(GURL(kAppUrl), AppLaunchInfo::GetLaunchWebURL(extension));
  EXPECT_EQ(GURL(kAppScope), GetScopeURLFromBookmarkApp(extension));
  EXPECT_FALSE(
      IconsInfo::GetIconResource(extension, kIconSizeTiny,
                                 ExtensionIconSet::MATCH_EXACTLY).empty());
  EXPECT_FALSE(
      IconsInfo::GetIconResource(
          extension, kIconSizeSmall, ExtensionIconSet::MATCH_EXACTLY).empty());
  EXPECT_FALSE(
      IconsInfo::GetIconResource(extension,
                                 kIconSizeSmall * 2,
                                 ExtensionIconSet::MATCH_EXACTLY).empty());
  EXPECT_FALSE(
      IconsInfo::GetIconResource(
          extension, kIconSizeMedium, ExtensionIconSet::MATCH_EXACTLY).empty());
  EXPECT_FALSE(
      IconsInfo::GetIconResource(extension,
                                 kIconSizeMedium * 2,
                                 ExtensionIconSet::MATCH_EXACTLY).empty());
}

// Deprecated in favour of
// InstallManagerBookmarkAppTest.InstallOrUpdateWebAppFromSync.
// TODO(crbug.com/915043): Erase it.
TEST_F(BookmarkAppHelperExtensionServiceTest, CreateAndUpdateBookmarkApp) {
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  WebApplicationInfo web_app_info;
  web_app_info.app_url = GURL(kAppUrl);
  web_app_info.title = base::UTF8ToUTF16(kAppTitle);
  web_app_info.description = base::UTF8ToUTF16(kAppDescription);
  web_app_info.scope = GURL(kAppScope);
  web_app_info.icons.push_back(
      CreateIconInfoWithBitmap(kIconSizeSmall, SK_ColorRED));

  extensions::CreateOrUpdateBookmarkApp(service_, &web_app_info,
                                        false /* is_locally_installed */);
  content::RunAllTasksUntilIdle();

  {
    EXPECT_EQ(1u, registry()->enabled_extensions().size());
    const Extension* extension =
        registry()->enabled_extensions().begin()->get();
    EXPECT_TRUE(extension->from_bookmark());
    EXPECT_EQ(kAppTitle, extension->name());
    EXPECT_EQ(kAppDescription, extension->description());
    EXPECT_EQ(GURL(kAppUrl), AppLaunchInfo::GetLaunchWebURL(extension));
    EXPECT_EQ(GURL(kAppScope), GetScopeURLFromBookmarkApp(extension));
    EXPECT_FALSE(extensions::IconsInfo::GetIconResource(
                     extension, kIconSizeSmall, ExtensionIconSet::MATCH_EXACTLY)
                     .empty());
    EXPECT_FALSE(BookmarkAppIsLocallyInstalled(profile(), extension));
  }

  web_app_info.title = base::UTF8ToUTF16(kAlternativeAppTitle);
  web_app_info.icons[0] = CreateIconInfoWithBitmap(kIconSizeLarge, SK_ColorRED);
  web_app_info.scope = GURL(kAppAlternativeScope);

  extensions::CreateOrUpdateBookmarkApp(service_, &web_app_info,
                                        true /* is_locally_installed */);
  content::RunAllTasksUntilIdle();

  {
    EXPECT_EQ(1u, registry()->enabled_extensions().size());
    const Extension* extension =
        registry()->enabled_extensions().begin()->get();
    EXPECT_TRUE(extension->from_bookmark());
    EXPECT_EQ(kAlternativeAppTitle, extension->name());
    EXPECT_EQ(kAppDescription, extension->description());
    EXPECT_EQ(GURL(kAppUrl), AppLaunchInfo::GetLaunchWebURL(extension));
    EXPECT_EQ(GURL(kAppAlternativeScope),
              GetScopeURLFromBookmarkApp(extension));
    EXPECT_FALSE(extensions::IconsInfo::GetIconResource(
                     extension, kIconSizeSmall, ExtensionIconSet::MATCH_EXACTLY)
                     .empty());
    EXPECT_FALSE(extensions::IconsInfo::GetIconResource(
                     extension, kIconSizeLarge, ExtensionIconSet::MATCH_EXACTLY)
                     .empty());
    EXPECT_TRUE(BookmarkAppIsLocallyInstalled(profile(), extension));
  }
}

}  // namespace extensions
