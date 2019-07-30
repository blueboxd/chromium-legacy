// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_icon_manager.h"

#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace web_app {

class WebAppIconManagerTest : public WebAppTest {
  void SetUp() override {
    WebAppTest::SetUp();

    auto file_utils = std::make_unique<TestFileUtils>();
    file_utils_ = file_utils.get();

    icon_manager_ =
        std::make_unique<WebAppIconManager>(profile(), std::move(file_utils));
  }

 protected:
  std::unique_ptr<WebAppIconManager> icon_manager_;

  // Owned by icon_manager_:
  TestFileUtils* file_utils_ = nullptr;
};

TEST_F(WebAppIconManagerTest, WriteAndReadIcon) {
  const GURL app_url = GURL("https://example.com/path");
  const AppId app_id = GenerateAppIdFromURL(app_url);

  const GURL icon_url = GURL("https://example.com/app.ico");
  const SkColor color = SK_ColorYELLOW;
  const int icon_size_px = icon_size::k512;

  // Write icon's bitmap to disk.
  {
    WebApplicationInfo::IconInfo icon_info =
        GenerateIconInfo(icon_url, icon_size_px, color);

    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->icons.push_back(std::move(icon_info));

    base::RunLoop run_loop;
    bool callback_called = false;

    icon_manager_->WriteData(app_id, std::move(web_app_info),
                             base::BindLambdaForTesting([&](bool success) {
                               callback_called = true;
                               EXPECT_TRUE(success);
                               run_loop.Quit();
                             }));

    run_loop.Run();
    EXPECT_TRUE(callback_called);
  }

  auto web_app = std::make_unique<WebApp>(app_id);

  WebApp::Icons icons;
  icons.push_back({icon_url, icon_size_px});
  web_app->SetIcons(std::move(icons));

  base::RunLoop run_loop;
  bool callback_called = false;

  const bool icon_requested = icon_manager_->ReadIcon(
      *web_app, icon_size_px, base::BindLambdaForTesting([&](SkBitmap bitmap) {
        callback_called = true;
        EXPECT_FALSE(bitmap.empty());
        EXPECT_EQ(color, bitmap.getColor(0, 0));
        run_loop.Quit();
      }));
  EXPECT_TRUE(icon_requested);

  run_loop.Run();
  EXPECT_TRUE(callback_called);
}

TEST_F(WebAppIconManagerTest, ReadIconFailed) {
  const GURL app_url = GURL("https://example.com/path");
  const AppId app_id = GenerateAppIdFromURL(app_url);

  const GURL icon_url = GURL("https://example.com/app.ico");
  const int icon_size_px = icon_size::k256;

  auto web_app = std::make_unique<WebApp>(app_id);

  // Set icon meta-info but don't write bitmap to disk.
  WebApp::Icons icons;
  icons.push_back({icon_url, icon_size_px});
  web_app->SetIcons(std::move(icons));

  // Request non-existing icon size.
  EXPECT_FALSE(
      icon_manager_->ReadIcon(*web_app, icon_size::k96, base::DoNothing()));

  // Request existing icon size which doesn't exist on disk.
  base::RunLoop run_loop;
  bool callback_called = false;

  const bool icon_requested = icon_manager_->ReadIcon(
      *web_app, icon_size_px, base::BindLambdaForTesting([&](SkBitmap bitmap) {
        callback_called = true;
        EXPECT_TRUE(bitmap.empty());
        run_loop.Quit();
      }));
  EXPECT_TRUE(icon_requested);

  run_loop.Run();
  EXPECT_TRUE(callback_called);
}

}  // namespace web_app
