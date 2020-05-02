// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_install_utils.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "url/gurl.h"

namespace web_app {

namespace {

const char kAppShortName[] = "Test short name";
const char kAppTitle[] = "Test title";
const char kAlternativeAppTitle[] = "Different test title";

// TODO(https://crbug.com/1042727): Fix test GURL scoping and remove this getter
// function.
GURL AppIcon1() {
  return GURL("fav1.png");
}
GURL AppIcon2() {
  return GURL("fav2.png");
}
GURL AppIcon3() {
  return GURL("fav3.png");
}
GURL AppUrl() {
  return GURL("http://www.chromium.org/index.html");
}
GURL AlternativeAppUrl() {
  return GURL("http://www.notchromium.org");
}

const char kShortcutItemName[] = "shortcut item ";

GURL ShortcutItemUrl() {
  return GURL("http://www.chromium.org/shortcuts/action");
}
GURL IconUrl1() {
  return GURL("http://www.chromium.org/shortcuts/icon1.png");
}
GURL IconUrl2() {
  return GURL("http://www.chromium.org/shortcuts/icon2.png");
}
GURL IconUrl3() {
  return GURL("http://www.chromium.org/shortcuts/icon3.png");
}

constexpr SquareSizePx kIconSize = 64;

// This value is greater than kMaxIcons in web_app_install_utils.cc.
constexpr unsigned int kNumTestIcons = 30;
}  // namespace

TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifest) {
  WebApplicationInfo web_app_info;
  web_app_info.title = base::UTF8ToUTF16(kAlternativeAppTitle);
  web_app_info.app_url = AlternativeAppUrl();
  WebApplicationIconInfo info;
  info.url = AppIcon1();
  web_app_info.icon_infos.push_back(info);

  blink::Manifest manifest;
  manifest.start_url = AppUrl();
  manifest.scope = AppUrl().GetWithoutFilename();
  manifest.short_name =
      base::NullableString16(base::UTF8ToUTF16(kAppShortName), false);

  {
    blink::Manifest::FileHandler handler;
    handler.action = GURL("http://example.com/open-files");
    handler.accept[base::UTF8ToUTF16("image/png")].push_back(
        base::UTF8ToUTF16(".png"));
    handler.name = base::UTF8ToUTF16("Images");
    manifest.file_handlers.push_back(handler);
  }

  UpdateWebAppInfoFromManifest(manifest, &web_app_info);
  EXPECT_EQ(base::UTF8ToUTF16(kAppShortName), web_app_info.title);
  EXPECT_EQ(AppUrl(), web_app_info.app_url);
  EXPECT_EQ(AppUrl().GetWithoutFilename(), web_app_info.scope);
  EXPECT_EQ(DisplayMode::kBrowser, web_app_info.display_mode);

  // The icon info from |web_app_info| should be left as is, since the manifest
  // doesn't have any icon information.
  EXPECT_EQ(1u, web_app_info.icon_infos.size());
  EXPECT_EQ(AppIcon1(), web_app_info.icon_infos[0].url);

  // Test that |manifest.name| takes priority over |manifest.short_name|, and
  // that icons provided by the manifest replace icons in |web_app_info|.
  manifest.name = base::NullableString16(base::UTF8ToUTF16(kAppTitle), false);
  manifest.display = DisplayMode::kMinimalUi;

  blink::Manifest::ImageResource icon;
  icon.src = AppIcon2();
  icon.purpose = {blink::Manifest::ImageResource::Purpose::ANY,
                  blink::Manifest::ImageResource::Purpose::BADGE};
  manifest.icons.push_back(icon);
  icon.src = AppIcon3();
  manifest.icons.push_back(icon);
  // Add an icon without purpose ANY (expect to be ignored).
  icon.purpose = {blink::Manifest::ImageResource::Purpose::BADGE};
  manifest.icons.push_back(icon);

  UpdateWebAppInfoFromManifest(manifest, &web_app_info);
  EXPECT_EQ(base::UTF8ToUTF16(kAppTitle), web_app_info.title);
  EXPECT_EQ(DisplayMode::kMinimalUi, web_app_info.display_mode);

  EXPECT_EQ(2u, web_app_info.icon_infos.size());
  EXPECT_EQ(AppIcon2(), web_app_info.icon_infos[0].url);
  EXPECT_EQ(AppIcon3(), web_app_info.icon_infos[1].url);

  // Check file handlers were updated
  EXPECT_EQ(1u, web_app_info.file_handlers.size());
  auto file_handler = web_app_info.file_handlers;
  EXPECT_EQ(manifest.file_handlers[0].action, file_handler[0].action);
  ASSERT_EQ(file_handler[0].accept.count(base::UTF8ToUTF16("image/png")), 1u);
  EXPECT_EQ(file_handler[0].accept[base::UTF8ToUTF16("image/png")][0],
            base::UTF8ToUTF16(".png"));
  EXPECT_EQ(file_handler[0].name, base::UTF8ToUTF16("Images"));
}

// Tests that WebAppInfo is correctly updated when Manifest contains Shortcuts.
TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifestWithShortcuts) {
  // Enable the feature.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kDesktopPWAsAppIconShortcutsMenu);
  WebApplicationInfo web_app_info;
  web_app_info.title = base::UTF8ToUTF16(kAlternativeAppTitle);
  web_app_info.app_url = AlternativeAppUrl();
  WebApplicationIconInfo info;
  info.url = AppIcon1();
  web_app_info.icon_infos.push_back(info);

  for (int i = 1; i < 4; ++i) {
    WebApplicationShortcutInfo shortcut_item;
    WebApplicationIconInfo icon;
    std::string shortcut_name = kShortcutItemName;
    shortcut_name += base::NumberToString(i);
    shortcut_item.name = base::UTF8ToUTF16(shortcut_name);
    shortcut_item.url = ShortcutItemUrl();

    icon.url = IconUrl1();
    icon.square_size_px = kIconSize;
    shortcut_item.shortcut_icon_infos.push_back(std::move(icon));
    web_app_info.shortcut_infos.push_back(shortcut_item);
  }

  blink::Manifest manifest;
  manifest.start_url = AppUrl();
  manifest.scope = AppUrl().GetWithoutFilename();
  manifest.short_name =
      base::NullableString16(base::UTF8ToUTF16(kAppShortName), false);

  {
    blink::Manifest::FileHandler handler;
    handler.action = GURL("http://example.com/open-files");
    handler.accept[base::UTF8ToUTF16("image/png")].push_back(
        base::UTF8ToUTF16(".png"));
    handler.name = base::UTF8ToUTF16("Images");
    manifest.file_handlers.push_back(handler);
  }

  UpdateWebAppInfoFromManifest(manifest, &web_app_info);
  EXPECT_EQ(base::UTF8ToUTF16(kAppShortName), web_app_info.title);
  EXPECT_EQ(AppUrl(), web_app_info.app_url);
  EXPECT_EQ(AppUrl().GetWithoutFilename(), web_app_info.scope);
  EXPECT_EQ(DisplayMode::kBrowser, web_app_info.display_mode);

  // The icon info from |web_app_info| should be left as is, since the manifest
  // doesn't have any icon information.
  EXPECT_EQ(1u, web_app_info.icon_infos.size());
  EXPECT_EQ(AppIcon1(), web_app_info.icon_infos[0].url);

  // The shortcut_infos from |web_app_info| should be left as is, since the
  // manifest doesn't have any shortcut information.
  EXPECT_EQ(3u, web_app_info.shortcut_infos.size());
  EXPECT_EQ(1u, web_app_info.shortcut_infos[0].shortcut_icon_infos.size());

  // Test that |manifest.name| takes priority over |manifest.short_name|, and
  // that icons provided by the manifest replace icons in |web_app_info|.
  manifest.name = base::NullableString16(base::UTF8ToUTF16(kAppTitle), false);
  manifest.display = DisplayMode::kMinimalUi;

  blink::Manifest::ImageResource icon;
  icon.src = AppIcon2();
  icon.purpose = {blink::Manifest::ImageResource::Purpose::ANY,
                  blink::Manifest::ImageResource::Purpose::BADGE};
  manifest.icons.push_back(icon);
  icon.src = AppIcon3();
  manifest.icons.push_back(icon);
  // Add an icon without purpose ANY (expect to be ignored).
  icon.purpose = {blink::Manifest::ImageResource::Purpose::BADGE};
  manifest.icons.push_back(icon);

  // Test that shortcuts in the manifest replace those in |web_app_info|.
  blink::Manifest::ShortcutItem shortcut_item;
  std::string shortcut_name = kShortcutItemName;
  shortcut_name += base::NumberToString(4);
  shortcut_item.name = base::UTF8ToUTF16(shortcut_name);
  shortcut_item.url = ShortcutItemUrl();

  icon.src = IconUrl2();
  icon.sizes.push_back(gfx::Size(10, 10));
  shortcut_item.icons.push_back(std::move(icon));

  manifest.shortcuts.push_back(shortcut_item);

  shortcut_name = kShortcutItemName;
  shortcut_name += base::NumberToString(5);
  shortcut_item.name = base::UTF8ToUTF16(shortcut_name);

  icon.src = IconUrl3();
  shortcut_item.icons.clear();
  shortcut_item.icons.push_back(std::move(icon));

  manifest.shortcuts.push_back(shortcut_item);

  UpdateWebAppInfoFromManifest(manifest, &web_app_info);
  EXPECT_EQ(base::UTF8ToUTF16(kAppTitle), web_app_info.title);
  EXPECT_EQ(DisplayMode::kMinimalUi, web_app_info.display_mode);

  EXPECT_EQ(2u, web_app_info.icon_infos.size());
  EXPECT_EQ(AppIcon2(), web_app_info.icon_infos[0].url);
  EXPECT_EQ(AppIcon3(), web_app_info.icon_infos[1].url);

  EXPECT_EQ(2u, web_app_info.shortcut_infos.size());
  EXPECT_EQ(1u, web_app_info.shortcut_infos[0].shortcut_icon_infos.size());
  WebApplicationIconInfo web_app_shortcut_icon =
      web_app_info.shortcut_infos[0].shortcut_icon_infos[0];
  EXPECT_EQ(IconUrl2(), web_app_shortcut_icon.url);

  EXPECT_EQ(1u, web_app_info.shortcut_infos[1].shortcut_icon_infos.size());
  web_app_shortcut_icon = web_app_info.shortcut_infos[1].shortcut_icon_infos[0];
  EXPECT_EQ(IconUrl3(), web_app_shortcut_icon.url);

  // Check file handlers were updated
  EXPECT_EQ(1u, web_app_info.file_handlers.size());
  auto file_handler = web_app_info.file_handlers;
  EXPECT_EQ(manifest.file_handlers[0].action, file_handler[0].action);
  ASSERT_EQ(file_handler[0].accept.count(base::UTF8ToUTF16("image/png")), 1u);
  EXPECT_EQ(file_handler[0].accept[base::UTF8ToUTF16("image/png")][0],
            base::UTF8ToUTF16(".png"));
  EXPECT_EQ(file_handler[0].name, base::UTF8ToUTF16("Images"));
}

// Tests that we limit the number of icons declared by a site.
TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifestTooManyIcons) {
  blink::Manifest manifest;
  for (int i = 0; i < 50; ++i) {
    blink::Manifest::ImageResource icon;
    icon.src = AppIcon1();
    icon.purpose.push_back(blink::Manifest::ImageResource::Purpose::ANY);
    icon.sizes.push_back(gfx::Size(i, i));
    manifest.icons.push_back(std::move(icon));
  }
  WebApplicationInfo web_app_info;

  UpdateWebAppInfoFromManifest(manifest, &web_app_info);
  EXPECT_EQ(20U, web_app_info.icon_infos.size());
}

// Tests that we limit the number of shortcut icons declared by a site.
TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifestTooManyShortcutIcons) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kDesktopPWAsAppIconShortcutsMenu);
  blink::Manifest manifest;
  for (unsigned int i = 0; i < kNumTestIcons; ++i) {
    blink::Manifest::ShortcutItem shortcut_item;
    std::string shortcut_name = kShortcutItemName;
    shortcut_name += base::NumberToString(i);
    shortcut_item.name = base::UTF8ToUTF16(shortcut_name);
    shortcut_item.url = ShortcutItemUrl();

    blink::Manifest::ImageResource icon;
    icon.src = IconUrl1();
    icon.sizes.push_back(gfx::Size(i, i));
    shortcut_item.icons.push_back(std::move(icon));

    manifest.shortcuts.push_back(shortcut_item);
  }
  WebApplicationInfo web_app_info;
  UpdateWebAppInfoFromManifest(manifest, &web_app_info);

  std::vector<WebApplicationIconInfo> all_icons;
  for (const auto& shortcut : web_app_info.shortcut_infos) {
    for (const auto& icon_info : shortcut.shortcut_icon_infos) {
      all_icons.push_back(std::move(icon_info));
    }
  }
  ASSERT_GT(kNumTestIcons, all_icons.size());
  EXPECT_EQ(20U, all_icons.size());
}

// Tests that we limit the size of icons declared by a site.
TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifestIconsTooLarge) {
  blink::Manifest manifest;
  for (int i = 1; i <= 20; ++i) {
    blink::Manifest::ImageResource icon;
    icon.src = AppIcon1();
    icon.purpose.push_back(blink::Manifest::ImageResource::Purpose::ANY);
    const int size = i * 100;
    icon.sizes.push_back(gfx::Size(size, size));
    manifest.icons.push_back(std::move(icon));
  }
  WebApplicationInfo web_app_info;
  UpdateWebAppInfoFromManifest(manifest, &web_app_info);

  EXPECT_EQ(10U, web_app_info.icon_infos.size());
  for (const WebApplicationIconInfo& icon : web_app_info.icon_infos) {
    EXPECT_LE(icon.square_size_px, 1024);
  }
}

// Tests that we limit the size of shortcut icons declared by a site.
TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifestShortcutIconsTooLarge) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kDesktopPWAsAppIconShortcutsMenu);
  blink::Manifest manifest;
  for (int i = 1; i <= 20; ++i) {
    blink::Manifest::ShortcutItem shortcut_item;
    std::string shortcut_name = kShortcutItemName;
    shortcut_name += base::NumberToString(i);
    shortcut_item.name = base::UTF8ToUTF16(shortcut_name);
    shortcut_item.url = ShortcutItemUrl();

    blink::Manifest::ImageResource icon;
    icon.src = IconUrl1();
    const int size = i * 100;
    icon.sizes.push_back(gfx::Size(size, size));
    shortcut_item.icons.push_back(std::move(icon));

    manifest.shortcuts.push_back(shortcut_item);
  }
  WebApplicationInfo web_app_info;
  UpdateWebAppInfoFromManifest(manifest, &web_app_info);

  std::vector<WebApplicationIconInfo> all_icons;
  for (const auto& shortcut : web_app_info.shortcut_infos) {
    for (const auto& icon_info : shortcut.shortcut_icon_infos) {
      all_icons.push_back(std::move(icon_info));
    }
  }
  EXPECT_EQ(10U, all_icons.size());
}

// Tests that SkBitmaps associated with shortcut item icons are populated in
// their own map in web_app_info.
TEST(WebAppInstallUtils, PopulateShortcutItemIcons) {
  WebApplicationInfo web_app_info;
  WebApplicationShortcutInfo shortcut_item;
  WebApplicationIconInfo icon;
  std::string shortcut_name = kShortcutItemName;
  shortcut_name += base::NumberToString(1);
  shortcut_item.name = base::UTF8ToUTF16(shortcut_name);
  shortcut_item.url = ShortcutItemUrl();
  icon.url = IconUrl1();
  icon.square_size_px = kIconSize;
  shortcut_item.shortcut_icon_infos.push_back(std::move(icon));
  web_app_info.shortcut_infos.push_back(std::move(shortcut_item));

  shortcut_name = kShortcutItemName;
  shortcut_name += base::NumberToString(2);
  shortcut_item.name = base::UTF8ToUTF16(shortcut_name);
  icon.url = IconUrl1();
  icon.square_size_px = kIconSize;
  shortcut_item.shortcut_icon_infos.push_back(std::move(icon));
  icon.url = IconUrl2();
  icon.square_size_px = 2 * kIconSize;
  shortcut_item.shortcut_icon_infos.push_back(std::move(icon));
  web_app_info.shortcut_infos.push_back(std::move(shortcut_item));

  IconsMap icons_map;
  std::vector<SkBitmap> bmp1 = {CreateSquareIcon(32, SK_ColorWHITE)};
  std::vector<SkBitmap> bmp2 = {CreateSquareIcon(32, SK_ColorBLUE)};
  std::vector<SkBitmap> bmp3 = {CreateSquareIcon(32, SK_ColorRED)};
  icons_map.emplace(IconUrl1(), bmp1);
  icons_map.emplace(IconUrl2(), bmp2);
  icons_map.emplace(AppIcon3(), bmp3);
  PopulateShortcutItemIcons(&web_app_info, &icons_map);

  // Ensure that reused shortcut icons are processed correctly.
  EXPECT_EQ(1U, web_app_info.shortcut_infos[0].shortcut_icon_bitmaps.size());
  EXPECT_EQ(2U, web_app_info.shortcut_infos[1].shortcut_icon_bitmaps.size());
}

// Tests that when PopulateShortcutItemIcons is called with no shortcut icon
// urls specified, no data is written to shortcut_infos.
TEST(WebAppInstallUtils, PopulateShortcutItemIconsNoShortcutIcons) {
  WebApplicationInfo web_app_info;
  IconsMap icons_map;
  std::vector<SkBitmap> bmp1 = {CreateSquareIcon(32, SK_ColorWHITE)};
  std::vector<SkBitmap> bmp2 = {CreateSquareIcon(32, SK_ColorBLUE)};
  std::vector<SkBitmap> bmp3 = {CreateSquareIcon(32, SK_ColorRED)};
  icons_map.emplace(IconUrl1(), bmp1);
  icons_map.emplace(IconUrl2(), bmp2);
  icons_map.emplace(IconUrl3(), bmp3);

  PopulateShortcutItemIcons(&web_app_info, &icons_map);

  EXPECT_EQ(0U, web_app_info.shortcut_infos.size());
}

}  // namespace web_app
