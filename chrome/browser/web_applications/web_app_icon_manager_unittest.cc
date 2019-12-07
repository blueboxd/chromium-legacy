// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_icon_manager.h"

#include <memory>

#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/test_web_app_database_factory.h"
#include "chrome/browser/web_applications/test/test_web_app_registry_controller.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

namespace web_app {

class WebAppIconManagerTest : public WebAppTest {
  void SetUp() override {
    WebAppTest::SetUp();

    test_registry_controller_ =
        std::make_unique<TestWebAppRegistryController>();
    test_registry_controller_->SetUp(profile());

    auto file_utils = std::make_unique<TestFileUtils>();
    file_utils_ = file_utils.get();

    icon_manager_ = std::make_unique<WebAppIconManager>(profile(), registrar(),
                                                        std::move(file_utils));

    controller().Init();
  }

 protected:
  void WriteIcons(const AppId& app_id,
                  const std::vector<int>& sizes_px,
                  const std::vector<SkColor>& colors) {
    DCHECK_EQ(sizes_px.size(), colors.size());

    auto web_app_info = std::make_unique<WebApplicationInfo>();

    for (size_t i = 0; i < sizes_px.size(); ++i) {
      std::string icon_name = base::StringPrintf("app-%d.ico", sizes_px[i]);
      AddGeneratedIcon(&web_app_info->icon_bitmaps, sizes_px[i], colors[i]);
    }

    base::RunLoop run_loop;
    icon_manager_->WriteData(app_id, std::move(web_app_info->icon_bitmaps),
                             base::BindLambdaForTesting([&](bool success) {
                               EXPECT_TRUE(success);
                               run_loop.Quit();
                             }));
    run_loop.Run();
  }

  std::vector<uint8_t> ReadSmallestCompressedIcon(const AppId& app_id,
                                                  int icon_size_in_px) {
    EXPECT_TRUE(icon_manager().HasSmallestIcon(app_id, icon_size_in_px));

    std::vector<uint8_t> result;

    base::RunLoop run_loop;
    icon_manager().ReadSmallestCompressedIcon(
        app_id, icon_size_in_px,
        base::BindLambdaForTesting([&](std::vector<uint8_t> data) {
          result = std::move(data);
          run_loop.Quit();
        }));

    run_loop.Run();
    return result;
  }

  std::unique_ptr<WebApp> CreateWebApp() {
    const GURL app_url = GURL("https://example.com/path");
    const AppId app_id = GenerateAppIdFromURL(app_url);

    auto web_app = std::make_unique<WebApp>(app_id);
    web_app->AddSource(Source::kSync);
    web_app->SetDisplayMode(DisplayMode::kStandalone);
    web_app->SetUserDisplayMode(DisplayMode::kStandalone);
    web_app->SetName("Name");
    web_app->SetLaunchUrl(app_url);

    return web_app;
  }

  TestWebAppRegistryController& controller() {
    return *test_registry_controller_;
  }

  WebAppRegistrar& registrar() { return controller().registrar(); }
  WebAppSyncBridge& sync_bridge() { return controller().sync_bridge(); }
  WebAppIconManager& icon_manager() { return *icon_manager_; }
  TestFileUtils& file_utils() {
    DCHECK(file_utils_);
    return *file_utils_;
  }

 private:
  std::unique_ptr<TestWebAppRegistryController> test_registry_controller_;
  std::unique_ptr<WebAppIconManager> icon_manager_;

  // Owned by icon_manager_:
  TestFileUtils* file_utils_ = nullptr;
};

TEST_F(WebAppIconManagerTest, WriteAndReadIcon) {
  auto web_app = CreateWebApp();
  const AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{icon_size::k512};
  const std::vector<SkColor> colors{SK_ColorYELLOW};
  WriteIcons(app_id, sizes_px, colors);

  web_app->SetDownloadedIconSizes(sizes_px);

  controller().RegisterApp(std::move(web_app));

  EXPECT_TRUE(icon_manager().HasIcon(app_id, sizes_px[0]));
  {
    base::RunLoop run_loop;

    icon_manager().ReadIcon(
        app_id, sizes_px[0],
        base::BindLambdaForTesting([&](const SkBitmap& bitmap) {
          EXPECT_FALSE(bitmap.empty());
          EXPECT_EQ(colors[0], bitmap.getColor(0, 0));
          run_loop.Quit();
        }));

    run_loop.Run();
  }
}

TEST_F(WebAppIconManagerTest, ReadAllIcons) {
  auto web_app = CreateWebApp();
  const AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{icon_size::k256, icon_size::k512};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  WriteIcons(app_id, sizes_px, colors);

  web_app->SetDownloadedIconSizes(sizes_px);

  controller().RegisterApp(std::move(web_app));
  {
    base::RunLoop run_loop;

    icon_manager().ReadAllIcons(
        app_id,
        base::BindLambdaForTesting(
            [&](std::map<SquareSizePx, SkBitmap> icons_map) {
              EXPECT_EQ(2u, icons_map.size());
              EXPECT_EQ(colors[0], icons_map[sizes_px[0]].getColor(0, 0));
              EXPECT_EQ(colors[1], icons_map[sizes_px[1]].getColor(0, 0));
              run_loop.Quit();
            }));

    run_loop.Run();
  }
}

TEST_F(WebAppIconManagerTest, ReadIconFailed) {
  auto web_app = CreateWebApp();
  const AppId app_id = web_app->app_id();

  const int icon_size_px = icon_size::k256;

  // Set icon meta-info but don't write bitmap to disk.
  web_app->SetDownloadedIconSizes({icon_size_px});

  controller().RegisterApp(std::move(web_app));

  // Check non-existing icon size.
  EXPECT_FALSE(icon_manager().HasIcon(app_id, icon_size::k96));

  EXPECT_TRUE(icon_manager().HasIcon(app_id, icon_size_px));

  // Request existing icon size which doesn't exist on disk.
  base::RunLoop run_loop;

  icon_manager().ReadIcon(
      app_id, icon_size_px,
      base::BindLambdaForTesting([&](const SkBitmap& bitmap) {
        EXPECT_TRUE(bitmap.empty());
        run_loop.Quit();
      }));

  run_loop.Run();
}

TEST_F(WebAppIconManagerTest, FindExact) {
  auto web_app = CreateWebApp();
  const AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{10, 60, 50, 20, 30};
  const std::vector<SkColor> colors{SK_ColorRED, SK_ColorYELLOW, SK_ColorGREEN,
                                    SK_ColorBLUE, SK_ColorMAGENTA};
  WriteIcons(app_id, sizes_px, colors);

  web_app->SetDownloadedIconSizes(sizes_px);

  controller().RegisterApp(std::move(web_app));

  EXPECT_FALSE(icon_manager().HasIcon(app_id, 40));

  {
    base::RunLoop run_loop;

    EXPECT_TRUE(icon_manager().HasIcon(app_id, 20));

    icon_manager().ReadIcon(
        app_id, 20, base::BindLambdaForTesting([&](const SkBitmap& bitmap) {
          EXPECT_FALSE(bitmap.empty());
          EXPECT_EQ(SK_ColorBLUE, bitmap.getColor(0, 0));
          run_loop.Quit();
        }));

    run_loop.Run();
  }
}

TEST_F(WebAppIconManagerTest, FindSmallest) {
  auto web_app = CreateWebApp();
  const AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{10, 60, 50, 20, 30};
  const std::vector<SkColor> colors{SK_ColorRED, SK_ColorYELLOW, SK_ColorGREEN,
                                    SK_ColorBLUE, SK_ColorMAGENTA};
  WriteIcons(app_id, sizes_px, colors);

  web_app->SetDownloadedIconSizes(sizes_px);

  controller().RegisterApp(std::move(web_app));

  EXPECT_FALSE(icon_manager().HasIcon(app_id, 70));

  {
    base::RunLoop run_loop;

    EXPECT_TRUE(icon_manager().HasSmallestIcon(app_id, 40));
    icon_manager().ReadSmallestIcon(
        app_id, 40, base::BindLambdaForTesting([&](const SkBitmap& bitmap) {
          EXPECT_FALSE(bitmap.empty());
          EXPECT_EQ(SK_ColorGREEN, bitmap.getColor(0, 0));
          run_loop.Quit();
        }));

    run_loop.Run();
  }

  {
    base::RunLoop run_loop;

    EXPECT_TRUE(icon_manager().HasSmallestIcon(app_id, 20));
    icon_manager().ReadSmallestIcon(
        app_id, 20, base::BindLambdaForTesting([&](const SkBitmap& bitmap) {
          EXPECT_FALSE(bitmap.empty());
          EXPECT_EQ(SK_ColorBLUE, bitmap.getColor(0, 0));
          run_loop.Quit();
        }));

    run_loop.Run();
  }
}

TEST_F(WebAppIconManagerTest, DeleteData_Success) {
  const AppId app1_id = GenerateAppIdFromURL(GURL("https://example.com/"));
  const AppId app2_id = GenerateAppIdFromURL(GURL("https://example.org/"));
  const GURL icons_root_url;  // url is empty to indicate autogenerated icons.

  const std::vector<int> sizes_px{icon_size::k128};
  const std::vector<SkColor> colors{SK_ColorMAGENTA};
  WriteIcons(app1_id, sizes_px, colors);
  WriteIcons(app2_id, sizes_px, colors);

  const base::FilePath web_apps_directory = GetWebAppsDirectory(profile());
  const base::FilePath app1_dir = web_apps_directory.AppendASCII(app1_id);
  const base::FilePath app2_dir = web_apps_directory.AppendASCII(app2_id);

  EXPECT_TRUE(file_utils().DirectoryExists(app1_dir));
  EXPECT_FALSE(file_utils().IsDirectoryEmpty(app1_dir));

  EXPECT_TRUE(file_utils().DirectoryExists(app2_dir));
  EXPECT_FALSE(file_utils().IsDirectoryEmpty(app2_dir));

  base::RunLoop run_loop;
  icon_manager().DeleteData(app2_id,
                            base::BindLambdaForTesting([&](bool success) {
                              EXPECT_TRUE(success);
                              run_loop.Quit();
                            }));
  run_loop.Run();

  EXPECT_TRUE(file_utils().DirectoryExists(web_apps_directory));

  EXPECT_TRUE(file_utils().DirectoryExists(app1_dir));
  EXPECT_FALSE(file_utils().IsDirectoryEmpty(app1_dir));

  EXPECT_FALSE(file_utils().DirectoryExists(app2_dir));
}

TEST_F(WebAppIconManagerTest, DeleteData_Failure) {
  const AppId app_id = GenerateAppIdFromURL(GURL("https://example.com/"));

  file_utils().SetNextDeleteFileRecursivelyResult(false);

  base::RunLoop run_loop;
  icon_manager().DeleteData(app_id,
                            base::BindLambdaForTesting([&](bool success) {
                              EXPECT_FALSE(success);
                              run_loop.Quit();
                            }));
  run_loop.Run();
}

TEST_F(WebAppIconManagerTest, ReadSmallestCompressedIcon_Success) {
  auto web_app = CreateWebApp();
  const AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{icon_size::k128};
  const std::vector<SkColor> colors{SK_ColorGREEN};
  WriteIcons(app_id, sizes_px, colors);

  web_app->SetDownloadedIconSizes(sizes_px);

  controller().RegisterApp(std::move(web_app));

  std::vector<uint8_t> compressed_data =
      ReadSmallestCompressedIcon(app_id, sizes_px[0]);
  EXPECT_FALSE(compressed_data.empty());

  auto* data_ptr = reinterpret_cast<const char*>(compressed_data.data());

  // Check that |compressed_data| starts with the 8-byte PNG magic string.
  std::string png_magic_string{data_ptr, 8};
  EXPECT_EQ(png_magic_string, "\x89\x50\x4e\x47\x0d\x0a\x1a\x0a");
}

TEST_F(WebAppIconManagerTest, ReadSmallestCompressedIcon_Failure) {
  auto web_app = CreateWebApp();
  const AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{icon_size::k64};
  web_app->SetDownloadedIconSizes(sizes_px);

  controller().RegisterApp(std::move(web_app));

  std::vector<uint8_t> compressed_data =
      ReadSmallestCompressedIcon(app_id, sizes_px[0]);
  EXPECT_TRUE(compressed_data.empty());
}

}  // namespace web_app
