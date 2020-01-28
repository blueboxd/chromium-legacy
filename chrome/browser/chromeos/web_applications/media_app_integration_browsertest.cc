// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/file_manager_test_util.h"
#include "chrome/browser/chromeos/file_manager/web_file_tasks.h"
#include "chrome/browser/chromeos/web_applications/system_web_app_integration_test.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
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
#include "extensions/browser/extension_system.h"
#include "testing/gtest/include/gtest/gtest.h"

using web_app::SystemAppType;

namespace {

// Path to a subfolder in chrome/test/data that holds test files.
constexpr base::FilePath::CharType kTestFilesFolderInTestData[] =
    FILE_PATH_LITERAL("chromeos/file_manager");

// An 800x600 image/png (all blue pixels).
constexpr char kFilePng800x600[] = "image.png";

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
    // Add the Files App, but remove the Gallery app, since its own file
    // handling may interfere. Long term, the Galley app will be removed.
    // TODO(crbug/1030935): Rely on flags alone to remove the Gallery App (i.e.
    // migrate this step to release code).
    extensions::ExtensionService* service =
        extensions::ExtensionSystem::Get(profile())->extension_service();
    service->component_loader()->Remove(file_manager::kGalleryAppId);
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

// Runs |script| in the unprivileged app frame of |web_ui|.
content::EvalJsResult EvalJsInAppFrame(content::WebContents* web_ui,
                                       const std::string& script) {
  // Clients of this helper all run in the same isolated world.
  constexpr int kWorldId = 1;

  // GetAllFrames does a breadth-first traversal. Assume the first subframe
  // is the app.
  std::vector<content::RenderFrameHost*> frames = web_ui->GetAllFrames();
  EXPECT_EQ(2u, frames.size());
  content::RenderFrameHost* app_frame = frames[1];
  EXPECT_TRUE(app_frame);

  return EvalJs(app_frame, script, content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                kWorldId);
}

void PrepareAppForTest(content::WebContents* web_ui) {
  EXPECT_TRUE(WaitForLoadStop(web_ui));
  EXPECT_EQ(nullptr, EvalJsInAppFrame(
                         web_ui, MediaAppUiBrowserTest::AppJsTestLibrary()));
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

  return EvalJsInAppFrame(web_ui, kScript);
}

}  // namespace

// Test that the Media App installs and launches correctly. Runs some spot
// checks on the manifest.
IN_PROC_BROWSER_TEST_F(MediaAppIntegrationTest, MediaApp) {
  const GURL url(chromeos::kChromeUIMediaAppURL);
  EXPECT_NO_FATAL_FAILURE(
      ExpectSystemWebAppValid(web_app::SystemAppType::MEDIA, url, "Media App"));
}

// Test that the MediaApp successfully loads a file passed in on its launch
// params.
IN_PROC_BROWSER_TEST_F(MediaAppIntegrationTest, MediaAppLaunchWithFile) {
  WaitForTestSystemAppInstall();
  auto params = LaunchParamsForApp(web_app::SystemAppType::MEDIA);

  // Add the 800x600 PNG image to launch params.
  params.launch_files.push_back(TestFile(kFilePng800x600));

  content::WebContents* app = LaunchApp(params);
  PrepareAppForTest(app);

  EXPECT_EQ("800x600", WaitForOpenedImage(app));

  // TODO(crbug/1027030): Add tests for re-launching with new files.
}

// Ensures that chrome://media-app is available as a file task for the ChromeOS
// file manager and eligible for opening appropriate files / mime types.
IN_PROC_BROWSER_TEST_F(MediaAppIntegrationTest, MediaAppElibibleOpenTask) {
  constexpr bool kIsDirectory = false;
  const extensions::EntryInfo test_entry(TestFile(kFilePng800x600), "image/png",
                                         kIsDirectory);

  WaitForTestSystemAppInstall();

  std::vector<file_manager::file_tasks::FullTaskDescriptor> result;
  file_manager::file_tasks::FindWebTasks(profile(), {test_entry}, &result);

  ASSERT_LT(0u, result.size());
  EXPECT_EQ(1u, result.size());
  const auto& task = result[0];
  const auto& descriptor = task.task_descriptor();

  EXPECT_EQ("Media App", task.task_title());
  EXPECT_EQ(extensions::api::file_manager_private::Verb::VERB_OPEN_WITH,
            task.task_verb());
  EXPECT_EQ(descriptor.app_id,
            *GetManager().GetAppIdForSystemApp(web_app::SystemAppType::MEDIA));
  EXPECT_EQ(chromeos::kChromeUIMediaAppURL, descriptor.action_id);
  EXPECT_EQ(file_manager::file_tasks::TASK_TYPE_WEB_APP, descriptor.task_type);
}

// End-to-end test to ensure that the MediaApp successfully registers as a file
// handler with the ChromeOS file manager on startup and acts as the default
// handler for a given file.
IN_PROC_BROWSER_TEST_F(MediaAppIntegrationWithFilesAppTest,
                       FileOpenUsesMediaApp) {
  WaitForTestSystemAppInstall();
  Browser* test_browser = chrome::FindBrowserWithActiveWindow();

  using platform_util::OpenOperationResult;

  file_manager::test::FolderInMyFiles folder(profile());
  folder.Add({TestFile(kFilePng800x600)});

  base::RunLoop run_loop;
  OpenOperationResult open_result;
  platform_util::OpenItem(
      profile(), folder.files()[0], platform_util::OPEN_FILE,
      base::BindLambdaForTesting([&](OpenOperationResult result) {
        open_result = result;
        run_loop.Quit();
      }));
  run_loop.Run();

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
