// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_install_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
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

}  // namespace web_app
