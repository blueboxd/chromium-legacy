// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/file_manager/file_manager_test_util.h"
#include "chrome/browser/chromeos/file_manager/web_file_tasks.h"
#include "chrome/browser/chromeos/web_applications/system_web_app_integration_test.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/components/media_app_ui/test/media_app_ui_browsertest.h"
#include "chromeos/components/media_app_ui/url_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/entry_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

using platform_util::OpenOperationResult;
using web_app::SystemAppType;

namespace {

// Path to a subfolder in chrome/test/data that holds test files.
constexpr base::FilePath::CharType kTestFilesFolderInTestData[] =
    FILE_PATH_LITERAL("chromeos/file_manager");

// An 800x600 image/png (all blue pixels).
constexpr char kFilePng800x600[] = "image.png";

// A 640x480 image/jpeg (all green pixels).
constexpr char kFileJpeg640x480[] = "image3.jpg";

// A 100x100 image/jpeg (all blue pixels).
constexpr char kFileJpeg100x100[] = "small.jpg";

// A 1-second long 648x486 VP9-encoded video with stereo Opus-encoded audio.
constexpr char kFileVideoVP9[] = "world.webm";

class MediaAppIntegrationTest : public SystemWebAppIntegrationTest {
 public:
  MediaAppIntegrationTest() {
    scoped_feature_list_.InitWithFeatures({chromeos::features::kMediaApp}, {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class MediaAppIntegrationWithFilesAppTest : public MediaAppIntegrationTest {
  void SetUpOnMainThread() override {
    file_manager::test::AddDefaultComponentExtensionsOnMainThread(profile());
    MediaAppIntegrationTest::SetUpOnMainThread();
  }
};

// Gets the base::FilePath for a named file in the test folder.
base::FilePath TestFile(const std::string& ascii_name) {
  base::FilePath path;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &path));
  path = path.Append(kTestFilesFolderInTestData);
  path = path.AppendASCII(ascii_name);

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(path));
  return path;
}

// Use platform_util::OpenItem() on the given |path| to simulate a user request
// to open that path, e.g., from the Files app or chrome://downloads.
OpenOperationResult OpenPathWithPlatformUtil(Profile* profile,
                                             const base::FilePath& path) {
  base::RunLoop run_loop;
  OpenOperationResult open_result;
  platform_util::OpenItem(
      profile, path, platform_util::OPEN_FILE,
      base::BindLambdaForTesting([&](OpenOperationResult result) {
        open_result = result;
        run_loop.Quit();
      }));
  run_loop.Run();
  return open_result;
}

void PrepareAppForTest(content::WebContents* web_ui) {
  EXPECT_TRUE(WaitForLoadStop(web_ui));
  EXPECT_EQ(nullptr, MediaAppUiBrowserTest::EvalJsInAppFrame(
                         web_ui, MediaAppUiBrowserTest::AppJsTestLibrary()));
}

content::WebContents* PrepareActiveBrowserForTest() {
  Browser* app_browser = chrome::FindBrowserWithActiveWindow();
  content::WebContents* web_ui =
      app_browser->tab_strip_model()->GetActiveWebContents();
  PrepareAppForTest(web_ui);
  return web_ui;
}

// Waits for a promise that resolves with image dimensions, once an <img>
// element appears in the light DOM that is backed by a blob URL.
content::EvalJsResult WaitForOpenedImage(content::WebContents* web_ui) {
  constexpr char kScript[] = R"(
      (async () => {
        const img = await waitForNode('img[src^="blob:"]');
        return `${img.naturalWidth}x${img.naturalHeight}`;
      })();
  )";

  return MediaAppUiBrowserTest::EvalJsInAppFrame(web_ui, kScript);
}

// Clears the `src` attribute of a blob:-backed <img> in the light DOM.
content::EvalJsResult ClearOpenedImage(content::WebContents* web_ui) {
  constexpr char kScript[] = R"(
      (async () => {
        const img = await waitForNode('img[src^="blob:"]');
        img.src = '';
        return true;
      })();
  )";

  return MediaAppUiBrowserTest::EvalJsInAppFrame(web_ui, kScript);
}

}  // namespace

// Test that the Media App installs and launches correctly. Runs some spot
// checks on the manifest.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest, MediaApp) {
  const GURL url(chromeos::kChromeUIMediaAppURL);
  EXPECT_NO_FATAL_FAILURE(
      ExpectSystemWebAppValid(web_app::SystemAppType::MEDIA, url, "Media App"));
}

// Test that the MediaApp successfully loads a file passed in on its launch
// params.
// Flaky. See https://crbug.com/1064863.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest,
                       DISABLED_MediaAppLaunchWithFile) {
  WaitForTestSystemAppInstall();
  auto params = LaunchParamsForApp(web_app::SystemAppType::MEDIA);

  // Add the 800x600 PNG image to launch params.
  params.launch_files.push_back(TestFile(kFilePng800x600));

  content::WebContents* app = LaunchApp(params);
  PrepareAppForTest(app);

  EXPECT_EQ("800x600", WaitForOpenedImage(app));

  // Clear the image, so that a new load can be reliably detected.
  EXPECT_EQ(true, ClearOpenedImage(app));

  // Relaunch with a different file. This currently re-uses the existing window.
  params.launch_files = {TestFile(kFileJpeg640x480)};
  LaunchApp(params);

  EXPECT_EQ("640x480", WaitForOpenedImage(app));
}

// Ensures that chrome://media-app is available as a file task for the ChromeOS
// file manager and eligible for opening appropriate files / mime types.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest, MediaAppEligibleOpenTask) {
  constexpr bool kIsDirectory = false;
  const extensions::EntryInfo image_entry(TestFile(kFilePng800x600),
                                          "image/png", kIsDirectory);
  const extensions::EntryInfo video_entry(TestFile(kFileVideoVP9), "video/webm",
                                          kIsDirectory);

  WaitForTestSystemAppInstall();

  for (const auto& single_entry : {video_entry, image_entry}) {
    SCOPED_TRACE(single_entry.mime_type);
    std::vector<file_manager::file_tasks::FullTaskDescriptor> result;
    file_manager::file_tasks::FindWebTasks(profile(), {single_entry}, &result);

    ASSERT_LT(0u, result.size());
    EXPECT_EQ(1u, result.size());
    const auto& task = result[0];
    const auto& descriptor = task.task_descriptor();

    EXPECT_EQ("Media App", task.task_title());
    EXPECT_EQ(extensions::api::file_manager_private::Verb::VERB_OPEN_WITH,
              task.task_verb());
    EXPECT_EQ(descriptor.app_id, *GetManager().GetAppIdForSystemApp(
                                     web_app::SystemAppType::MEDIA));
    EXPECT_EQ(chromeos::kChromeUIMediaAppURL, descriptor.action_id);
    EXPECT_EQ(file_manager::file_tasks::TASK_TYPE_WEB_APP,
              descriptor.task_type);
  }
}

IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest, HiddenInLauncherAndSearch) {
  WaitForTestSystemAppInstall();

  // Check system_web_app_manager has the correct attributes for Media App.
  EXPECT_FALSE(
      GetManager().ShouldShowInLauncher(web_app::SystemAppType::MEDIA));
  EXPECT_FALSE(GetManager().ShouldShowInSearch(web_app::SystemAppType::MEDIA));
}

// End-to-end test to ensure that the MediaApp successfully registers as a file
// handler with the ChromeOS file manager on startup and acts as the default
// handler for a given file.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationWithFilesAppTest,
                       FileOpenUsesMediaApp) {
  WaitForTestSystemAppInstall();
  Browser* test_browser = chrome::FindBrowserWithActiveWindow();

  file_manager::test::FolderInMyFiles folder(profile());
  folder.Add({TestFile(kFilePng800x600)});

  OpenOperationResult open_result =
      OpenPathWithPlatformUtil(profile(), folder.files()[0]);

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(browser()->profile());
  proxy->FlushMojoCallsForTesting();

  // Window focus changes on ChromeOS are synchronous, so just get the newly
  // focused window.
  Browser* app_browser = chrome::FindBrowserWithActiveWindow();
  content::WebContents* web_ui =
      app_browser->tab_strip_model()->GetActiveWebContents();
  PrepareAppForTest(web_ui);

  EXPECT_EQ(open_result, platform_util::OPEN_SUCCEEDED);

  // Check that chrome://media-app launched and the test file loads.
  EXPECT_NE(test_browser, app_browser);
  EXPECT_EQ(web_app::GetAppIdFromApplicationName(app_browser->app_name()),
            *GetManager().GetAppIdForSystemApp(web_app::SystemAppType::MEDIA));
  EXPECT_EQ("800x600", WaitForOpenedImage(web_ui));
}

// Test that the MediaApp can navigate other files in the directory of a file
// that was opened.
// Flaky. See https://crbug.com/1064864.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationWithFilesAppTest,
                       DISABLED_FileOpenCanTraverseDirectory) {
  WaitForTestSystemAppInstall();

  // Initialize a folder with 3 files: 2 JPEG, 1 PNG. Note this approach doesn't
  // guarantee the modification times of the files so, and therefore does not
  // suggest an ordering to the files of the directory contents. But by having
  // at most two active files, we can still write a robust test.
  file_manager::test::FolderInMyFiles folder(profile());
  folder.Add({
      TestFile(kFilePng800x600),
      TestFile(kFileJpeg640x480),
      TestFile(kFileJpeg100x100),
  });

  const base::FilePath copied_png_800x600 = folder.files()[0];
  const base::FilePath copied_jpeg_640x480 = folder.files()[1];

  // Sent an open request using only the 640x480 JPEG file.
  OpenPathWithPlatformUtil(profile(), copied_jpeg_640x480);
  content::WebContents* web_ui = PrepareActiveBrowserForTest();

  EXPECT_EQ("640x480", WaitForOpenedImage(web_ui));

  // Clear the <img> src attribute to ensure we can detect changes reliably.
  // TODO(crbug.com/893226): Use the alt-text to find the image instead.
  ClearOpenedImage(web_ui);

  // Navigate to the next file in the directory.
  EXPECT_EQ(true, ExecuteScript(web_ui, "advance(1)"));
  EXPECT_EQ("100x100", WaitForOpenedImage(web_ui));

  // Navigating again should wraparound, but skip the 800x600 PNG because it is
  // a different mime type to the original open request.
  ClearOpenedImage(web_ui);
  EXPECT_EQ(true, ExecuteScript(web_ui, "advance(1)"));
  EXPECT_EQ("640x480", WaitForOpenedImage(web_ui));

  // Navigate backwards.
  ClearOpenedImage(web_ui);
  EXPECT_EQ(true, ExecuteScript(web_ui, "advance(-1)"));
  EXPECT_EQ("100x100", WaitForOpenedImage(web_ui));

  // Now open the png.
  ClearOpenedImage(web_ui);
  OpenPathWithPlatformUtil(profile(), copied_png_800x600);
  EXPECT_EQ("800x600", WaitForOpenedImage(web_ui));

  // Navigating should stay on this file. Note currently, this will "reload" the
  // file. It would also be acceptable to "do nothing", but that will be tackled
  // on the UI layer by hiding the buttons.
  ClearOpenedImage(web_ui);
  EXPECT_EQ(true, ExecuteScript(web_ui, "advance(1)"));
  EXPECT_EQ("800x600", WaitForOpenedImage(web_ui));
}

INSTANTIATE_TEST_SUITE_P(All,
                         MediaAppIntegrationTest,
                         ::testing::Values(web_app::ProviderType::kBookmarkApps,
                                           web_app::ProviderType::kWebApps),
                         web_app::ProviderTypeParamToString);

INSTANTIATE_TEST_SUITE_P(All,
                         MediaAppIntegrationWithFilesAppTest,
                         ::testing::Values(web_app::ProviderType::kBookmarkApps,
                                           web_app::ProviderType::kWebApps),
                         web_app::ProviderTypeParamToString);
