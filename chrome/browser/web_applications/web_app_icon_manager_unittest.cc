// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_icon_manager.h"

#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/test_web_app_sync_bridge.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace web_app {

class WebAppIconManagerTest : public WebAppTest {
  void SetUp() override {
    WebAppTest::SetUp();

    auto file_utils = std::make_unique<TestFileUtils>();
    file_utils_ = file_utils.get();

    sync_bridge_ = std::make_unique<TestWebAppSyncBridge>();
    registrar_ = std::make_unique<WebAppRegistrar>(nullptr, sync_bridge_.get());
    icon_manager_ = std::make_unique<WebAppIconManager>(profile(), *registrar_,
                                                        std::move(file_utils));
  }

 protected:
  void WriteIcons(const AppId& app_id,
                  const GURL& app_url,
                  const std::vector<int>& sizes_px,
                  const std::vector<SkColor>& colors) {
    DCHECK_EQ(sizes_px.size(), colors.size());
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->icons.reserve(sizes_px.size());
    for (size_t i = 0; i < sizes_px.size(); ++i) {
      std::string icon_name = base::StringPrintf("app-%d.ico", sizes_px[i]);
      GURL icon_url = app_url.Resolve(icon_name);
      web_app_info->icons.push_back(
          GenerateIconInfo(icon_url, sizes_px[i], colors[i]));
    }

    base::RunLoop run_loop;
    icon_manager_->WriteData(app_id, std::move(web_app_info),
                             base::BindLambdaForTesting([&](bool success) {
                               EXPECT_TRUE(success);
                               run_loop.Quit();
                             }));
    run_loop.Run();
  }

  WebApp::Icons ListIcons(const GURL& app_url,
                          const std::vector<int>& sizes_px) {
    WebApp::Icons icons;
    icons.reserve(sizes_px.size());
    for (size_t i = 0; i < sizes_px.size(); ++i) {
      std::string icon_name = base::StringPrintf("app-%d.ico", sizes_px[i]);
      GURL icon_url = app_url.Resolve(icon_name);
      icons.push_back({icon_url, sizes_px[i]});
    }
    return icons;
  }

  std::unique_ptr<TestWebAppSyncBridge> sync_bridge_;
  std::unique_ptr<WebAppRegistrar> registrar_;
  std::unique_ptr<WebAppIconManager> icon_manager_;

  // Owned by icon_manager_:
  TestFileUtils* file_utils_ = nullptr;
};

TEST_F(WebAppIconManagerTest, WriteAndReadIcon) {
  const std::string name = "Name";
  const GURL app_url = GURL("https://example.com/path");
  const AppId app_id = GenerateAppIdFromURL(app_url);

  const std::vector<int> sizes_px{icon_size::k512};
  const std::vector<SkColor> colors{SK_ColorYELLOW};
  WriteIcons(app_id, app_url, sizes_px, colors);

  auto web_app = std::make_unique<WebApp>(app_id);
  web_app->SetName(name);
  web_app->SetLaunchUrl(app_url);
  web_app->SetIcons(ListIcons(app_url, sizes_px));
  registrar_->RegisterApp(std::move(web_app));

  {
    base::RunLoop run_loop;

    const bool icon_requested = icon_manager_->ReadIcon(
        app_id, sizes_px[0], base::BindLambdaForTesting([&](SkBitmap bitmap) {
          EXPECT_FALSE(bitmap.empty());
          EXPECT_EQ(colors[0], bitmap.getColor(0, 0));
          run_loop.Quit();
        }));
    EXPECT_TRUE(icon_requested);

    run_loop.Run();
  }
}

TEST_F(WebAppIconManagerTest, ReadIconFailed) {
  const std::string name = "Name";
  const GURL app_url = GURL("https://example.com/path");
  const AppId app_id = GenerateAppIdFromURL(app_url);

  const GURL icon_url = GURL("https://example.com/app.ico");
  const int icon_size_px = icon_size::k256;

  auto web_app = std::make_unique<WebApp>(app_id);
  web_app->SetName(name);
  web_app->SetLaunchUrl(app_url);

  // Set icon meta-info but don't write bitmap to disk.
  WebApp::Icons icons;
  icons.push_back({icon_url, icon_size_px});
  web_app->SetIcons(std::move(icons));
  registrar_->RegisterApp(std::move(web_app));

  // Request non-existing icon size.
  EXPECT_FALSE(
      icon_manager_->ReadIcon(app_id, icon_size::k96, base::DoNothing()));

  // Request existing icon size which doesn't exist on disk.
  base::RunLoop run_loop;

  const bool icon_requested = icon_manager_->ReadIcon(
      app_id, icon_size_px, base::BindLambdaForTesting([&](SkBitmap bitmap) {
        EXPECT_TRUE(bitmap.empty());
        run_loop.Quit();
      }));
  EXPECT_TRUE(icon_requested);

  run_loop.Run();
}

TEST_F(WebAppIconManagerTest, FindExact) {
  const std::string name = "Name";
  const GURL app_url = GURL("https://example.com/path");
  const AppId app_id = GenerateAppIdFromURL(app_url);

  const std::vector<int> sizes_px{10, 60, 50, 20, 30};
  const std::vector<SkColor> colors{SK_ColorRED, SK_ColorYELLOW, SK_ColorGREEN,
                                    SK_ColorBLUE, SK_ColorMAGENTA};
  WriteIcons(app_id, app_url, sizes_px, colors);

  auto web_app = std::make_unique<WebApp>(app_id);
  web_app->SetName(name);
  web_app->SetLaunchUrl(app_url);
  web_app->SetIcons(ListIcons(app_url, sizes_px));
  registrar_->RegisterApp(std::move(web_app));

  {
    const bool icon_requested = icon_manager_->ReadIcon(
        app_id, 40,
        base::BindLambdaForTesting([&](SkBitmap bitmap) { NOTREACHED(); }));
    EXPECT_FALSE(icon_requested);
  }

  {
    base::RunLoop run_loop;

    const bool icon_requested = icon_manager_->ReadIcon(
        app_id, 20, base::BindLambdaForTesting([&](SkBitmap bitmap) {
          EXPECT_FALSE(bitmap.empty());
          EXPECT_EQ(SK_ColorBLUE, bitmap.getColor(0, 0));
          run_loop.Quit();
        }));
    EXPECT_TRUE(icon_requested);

    run_loop.Run();
  }
}

TEST_F(WebAppIconManagerTest, FindSmallest) {
  const std::string name = "Name";
  const GURL app_url = GURL("https://example.com/path");
  const AppId app_id = GenerateAppIdFromURL(app_url);

  const std::vector<int> sizes_px{10, 60, 50, 20, 30};
  const std::vector<SkColor> colors{SK_ColorRED, SK_ColorYELLOW, SK_ColorGREEN,
                                    SK_ColorBLUE, SK_ColorMAGENTA};
  WriteIcons(app_id, app_url, sizes_px, colors);

  auto web_app = std::make_unique<WebApp>(app_id);
  web_app->SetName(name);
  web_app->SetLaunchUrl(app_url);
  web_app->SetIcons(ListIcons(app_url, sizes_px));
  registrar_->RegisterApp(std::move(web_app));

  {
    const bool icon_requested = icon_manager_->ReadSmallestIcon(
        app_id, 70,
        base::BindLambdaForTesting([&](SkBitmap bitmap) { NOTREACHED(); }));
    EXPECT_FALSE(icon_requested);
  }

  {
    base::RunLoop run_loop;

    const bool icon_requested = icon_manager_->ReadSmallestIcon(
        app_id, 40, base::BindLambdaForTesting([&](SkBitmap bitmap) {
          EXPECT_FALSE(bitmap.empty());
          EXPECT_EQ(SK_ColorGREEN, bitmap.getColor(0, 0));
          run_loop.Quit();
        }));
    EXPECT_TRUE(icon_requested);

    run_loop.Run();
  }

  {
    base::RunLoop run_loop;

    const bool icon_requested = icon_manager_->ReadSmallestIcon(
        app_id, 20, base::BindLambdaForTesting([&](SkBitmap bitmap) {
          EXPECT_FALSE(bitmap.empty());
          EXPECT_EQ(SK_ColorBLUE, bitmap.getColor(0, 0));
          run_loop.Quit();
        }));
    EXPECT_TRUE(icon_requested);

    run_loop.Run();
  }
}

}  // namespace web_app
