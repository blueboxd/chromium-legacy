// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_icon_manager.h"

#include <algorithm>
#include <memory>

#include "base/bind_helpers.h"
#include "base/files/file_enumerator.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/test_web_app_database_factory.h"
#include "chrome/browser/web_applications/test/test_web_app_registry_controller.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/common/constants.h"
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
                             std::vector<std::map<SquareSizePx, SkBitmap>>(),
                             base::BindLambdaForTesting([&](bool success) {
                               EXPECT_TRUE(success);
                               run_loop.Quit();
                             }));
    run_loop.Run();
  }

  void WriteShortcutIcons(const AppId& app_id,
                          const std::vector<int>& sizes_px,
                          const std::vector<SkColor>& colors) {
    DCHECK_EQ(sizes_px.size(), colors.size());
    std::vector<std::map<SquareSizePx, SkBitmap>> shortcut_icons;
    for (size_t i = 0; i < sizes_px.size(); i++) {
      std::map<SquareSizePx, SkBitmap> shortcut_icon_map;
      std::vector<SquareSizePx> icon_sizes;
      shortcut_icon_map.emplace(sizes_px[i],
                                CreateSquareIcon(sizes_px[i], colors[i]));
      shortcut_icons.push_back(std::move(shortcut_icon_map));
    }

    base::RunLoop run_loop;
    icon_manager_->WriteData(app_id, std::map<SquareSizePx, SkBitmap>(),
                             shortcut_icons,
                             base::BindLambdaForTesting([&](bool success) {
                               EXPECT_TRUE(success);
                               run_loop.Quit();
                             }));
    run_loop.Run();
  }

  std::vector<std::vector<SquareSizePx>> CreateDownloadedShortcutIconsSizes(
      std::vector<SquareSizePx> sizes_px) {
    std::vector<std::vector<SquareSizePx>> downloaded_shortcut_icons_sizes;
    for (const auto& size : sizes_px) {
      std::vector<SquareSizePx> icon_sizes;
      icon_sizes.push_back(size);
      downloaded_shortcut_icons_sizes.push_back(std::move(icon_sizes));
    }
    return downloaded_shortcut_icons_sizes;
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

  SkColor ReadIconAndResize(const AppId& app_id, int desired_icon_size) {
    base::RunLoop run_loop;
    SkColor icon_color = SK_ColorBLACK;

    icon_manager().ReadIconAndResize(
        app_id, desired_icon_size,
        base::BindLambdaForTesting(
            [&](std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
              EXPECT_EQ(1u, icon_bitmaps.size());
              SkBitmap bitmap = icon_bitmaps[desired_icon_size];
              EXPECT_FALSE(bitmap.empty());
              EXPECT_EQ(desired_icon_size, bitmap.width());
              EXPECT_EQ(desired_icon_size, bitmap.height());
              icon_color = bitmap.getColor(0, 0);

              run_loop.Quit();
            }));

    run_loop.Run();
    return icon_color;
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

TEST_F(WebAppIconManagerTest, WriteAndReadIcons) {
  auto web_app = CreateWebApp();
  const AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{icon_size::k256, icon_size::k512};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  WriteIcons(app_id, sizes_px, colors);

  web_app->SetDownloadedIconSizes(sizes_px);

  controller().RegisterApp(std::move(web_app));

  EXPECT_TRUE(icon_manager().HasIcons(app_id, sizes_px));
  {
    base::RunLoop run_loop;

    icon_manager().ReadIcons(
        app_id, sizes_px,
        base::BindLambdaForTesting(
            [&](std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
              EXPECT_EQ(2u, icon_bitmaps.size());

              EXPECT_FALSE(icon_bitmaps[icon_size::k256].empty());
              EXPECT_EQ(SK_ColorGREEN,
                        icon_bitmaps[icon_size::k256].getColor(0, 0));

              EXPECT_FALSE(icon_bitmaps[icon_size::k512].empty());
              EXPECT_EQ(SK_ColorYELLOW,
                        icon_bitmaps[icon_size::k512].getColor(0, 0));

              run_loop.Quit();
            }));

    run_loop.Run();
  }
}

TEST_F(WebAppIconManagerTest, OverwriteIcons) {
  auto web_app = CreateWebApp();
  const AppId app_id = web_app->app_id();

  // Write initial red icons to be overwritten.
  {
    std::vector<int> sizes_px{icon_size::k32, icon_size::k64, icon_size::k48};
    const std::vector<SkColor> colors{SK_ColorRED, SK_ColorRED, SK_ColorRED};
    WriteIcons(app_id, sizes_px, colors);

    web_app->SetDownloadedIconSizes(std::move(sizes_px));
  }

  controller().RegisterApp(std::move(web_app));

  // k64 and k48 sizes to be overwritten. Skip k32 size and add new k96 size.
  const std::vector<int> overwritten_sizes_px{icon_size::k48, icon_size::k64,
                                              icon_size::k96};
  {
    std::map<SquareSizePx, SkBitmap> icon_bitmaps;
    for (int size_px : overwritten_sizes_px)
      icon_bitmaps[size_px] = CreateSquareIcon(size_px, SK_ColorGREEN);

    base::RunLoop run_loop;

    // Overwrite red icons with green ones.
    icon_manager().WriteData(app_id, std::move(icon_bitmaps),
                             std::vector<std::map<SquareSizePx, SkBitmap>>(),
                             base::BindLambdaForTesting([&](bool success) {
                               EXPECT_TRUE(success);
                               run_loop.Quit();
                             }));

    run_loop.Run();

    ScopedRegistryUpdate update(&controller().sync_bridge());
    update->UpdateApp(app_id)->SetDownloadedIconSizes(overwritten_sizes_px);
  }

  // Check that all icons are now green. Check that all red icons were deleted
  // on disk (including the k32 size).
  base::FilePath icons_dir = GetAppIconsDir(profile(), app_id);

  std::vector<int> sizes_on_disk_px;

  base::FileEnumerator enumerator(icons_dir, true, base::FileEnumerator::FILES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    EXPECT_TRUE(path.MatchesExtension(FILE_PATH_LITERAL(".png")));

    SkBitmap bitmap;
    EXPECT_TRUE(ReadBitmap(&file_utils(), path, &bitmap));
    EXPECT_FALSE(bitmap.empty());
    EXPECT_EQ(bitmap.width(), bitmap.height());
    EXPECT_EQ(SK_ColorGREEN, bitmap.getColor(0, 0));

    sizes_on_disk_px.push_back(bitmap.width());
  }

  std::sort(sizes_on_disk_px.begin(), sizes_on_disk_px.end());
  EXPECT_EQ(overwritten_sizes_px, sizes_on_disk_px);
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

TEST_F(WebAppIconManagerTest, WriteAndReadAllShortcutIcons) {
  auto web_app = CreateWebApp();
  const AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px = {icon_size::k64, icon_size::k128,
                                     icon_size::k256};
  const std::vector<SkColor> colors = {SK_ColorRED, SK_ColorWHITE,
                                       SK_ColorBLUE};

  WriteShortcutIcons(app_id, sizes_px, colors);

  web_app->SetDownloadedShortcutIconsSizes(
      CreateDownloadedShortcutIconsSizes(sizes_px));

  controller().RegisterApp(std::move(web_app));
  {
    base::RunLoop run_loop;

    icon_manager().ReadAllShortcutIcons(
        app_id,
        base::BindLambdaForTesting(
            [&](std::vector<std::map<SquareSizePx, SkBitmap>>
                    shortcut_icons_map) {
              EXPECT_EQ(3u, shortcut_icons_map.size());
              EXPECT_EQ(sizes_px[0], shortcut_icons_map[0].begin()->first);
              EXPECT_EQ(colors[0],
                        shortcut_icons_map[0].begin()->second.getColor(0, 0));
              EXPECT_EQ(sizes_px[1], shortcut_icons_map[1].begin()->first);
              EXPECT_EQ(colors[1],
                        shortcut_icons_map[1].begin()->second.getColor(0, 0));
              EXPECT_EQ(sizes_px[2], shortcut_icons_map[2].begin()->first);
              EXPECT_EQ(colors[2],
                        shortcut_icons_map[2].begin()->second.getColor(0, 0));
              run_loop.Quit();
            }));

    run_loop.Run();
  }
}

TEST_F(WebAppIconManagerTest, ReadIconsFailed) {
  auto web_app = CreateWebApp();
  const AppId app_id = web_app->app_id();

  const std::vector<SquareSizePx> icon_sizes_px{icon_size::k256};

  // Set icon meta-info but don't write bitmap to disk.
  web_app->SetDownloadedIconSizes(icon_sizes_px);

  controller().RegisterApp(std::move(web_app));

  EXPECT_FALSE(icon_manager().HasIcons(app_id, {icon_size::k96}));
  EXPECT_TRUE(icon_manager().HasIcons(app_id, {icon_size::k256}));
  EXPECT_FALSE(
      icon_manager().HasIcons(app_id, {icon_size::k96, icon_size::k256}));

  // Request existing icon size which doesn't exist on disk.
  base::RunLoop run_loop;

  icon_manager().ReadIcons(
      app_id, icon_sizes_px,
      base::BindLambdaForTesting(
          [&](std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
            EXPECT_TRUE(icon_bitmaps.empty());
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

  EXPECT_FALSE(icon_manager().HasIcons(app_id, {40}));

  {
    base::RunLoop run_loop;

    EXPECT_TRUE(icon_manager().HasIcons(app_id, {20}));

    icon_manager().ReadIcons(
        app_id, {20},
        base::BindLambdaForTesting(
            [&](std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
              EXPECT_EQ(1u, icon_bitmaps.size());
              EXPECT_FALSE(icon_bitmaps[20].empty());
              EXPECT_EQ(SK_ColorBLUE, icon_bitmaps[20].getColor(0, 0));
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

  EXPECT_FALSE(icon_manager().HasSmallestIcon(app_id, 70));

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

  const std::vector<int> sizes_px{icon_size::k128};
  const std::vector<SkColor> colors{SK_ColorMAGENTA};
  WriteIcons(app1_id, sizes_px, colors);
  WriteIcons(app2_id, sizes_px, colors);

  const base::FilePath web_apps_root_directory =
      GetWebAppsRootDirectory(profile());
  const base::FilePath app1_dir =
      GetManifestResourcesDirectoryForApp(web_apps_root_directory, app1_id);
  const base::FilePath app2_dir =
      GetManifestResourcesDirectoryForApp(web_apps_root_directory, app2_id);

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

  base::FilePath manifest_resources_directory =
      GetManifestResourcesDirectory(web_apps_root_directory);
  EXPECT_TRUE(file_utils().DirectoryExists(manifest_resources_directory));

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

TEST_F(WebAppIconManagerTest, ReadIconAndResize_Success) {
  auto web_app = CreateWebApp();
  const AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{icon_size::k32, icon_size::k64,
                                  icon_size::k256, icon_size::k512};
  const std::vector<SkColor> colors{SK_ColorBLUE, SK_ColorGREEN, SK_ColorYELLOW,
                                    SK_ColorRED};
  WriteIcons(app_id, sizes_px, colors);

  web_app->SetDownloadedIconSizes(sizes_px);

  controller().RegisterApp(std::move(web_app));

  for (SquareSizePx size : sizes_px)
    EXPECT_TRUE(icon_manager().HasIconToResize(app_id, size));

  EXPECT_TRUE(icon_manager().HasIconToResize(app_id, icon_size::k128));
  EXPECT_TRUE(icon_manager().HasIconToResize(app_id, icon_size::k16));
  EXPECT_TRUE(icon_manager().HasIconToResize(app_id, 1024));

  for (size_t i = 0; i < sizes_px.size(); ++i)
    EXPECT_EQ(colors[i], ReadIconAndResize(app_id, sizes_px[i]));

  EXPECT_EQ(SK_ColorYELLOW, ReadIconAndResize(app_id, icon_size::k128));
  EXPECT_EQ(SK_ColorBLUE, ReadIconAndResize(app_id, icon_size::k16));
  EXPECT_EQ(SK_ColorRED, ReadIconAndResize(app_id, 1024));
}

TEST_F(WebAppIconManagerTest, ReadIconAndResize_Failure) {
  auto web_app = CreateWebApp();
  const AppId app_id = web_app->app_id();

  web_app->SetDownloadedIconSizes({icon_size::k32, icon_size::k64});

  controller().RegisterApp(std::move(web_app));

  base::RunLoop run_loop;

  icon_manager().ReadIconAndResize(
      app_id, icon_size::k128,
      base::BindLambdaForTesting(
          [&](std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
            EXPECT_TRUE(icon_bitmaps.empty());
            run_loop.Quit();
          }));

  run_loop.Run();
}

TEST_F(WebAppIconManagerTest, MatchSizes) {
  EXPECT_EQ(kWebAppIconSmall, extension_misc::EXTENSION_ICON_SMALL);
}

}  // namespace web_app
