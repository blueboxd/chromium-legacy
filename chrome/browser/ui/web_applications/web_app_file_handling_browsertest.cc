// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/launch_service/launch_service.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/common/web_application_info.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "third_party/blink/public/common/features.h"

class WebAppFileHandlingBrowserTest
    : public web_app::WebAppControllerBrowserTest {
 public:
  WebAppFileHandlingBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kNativeFileSystemAPI,
         blink::features::kFileHandlingAPI},
        {});
  }

  GURL GetSecureAppURL() {
    return https_server()->GetURL("app.com", "/ssl/google.html");
  }

  GURL GetTextFileHandlerActionURL() {
    return https_server()->GetURL("app.com", "/ssl/blank_page.html");
  }

  GURL GetCSVFileHandlerActionURL() {
    return https_server()->GetURL("app.com", "/ssl/page_with_refs.html");
  }

  base::FilePath NewTestFilePath(const base::FilePath::CharType* extension) {
    // CreateTemporaryFile blocks, temporarily allow blocking.
    base::ScopedAllowBlockingForTesting allow_blocking;

    // In order to test file handling, we need to be able to supply a file
    // extension for the temp file.
    base::FilePath test_file_path;
    base::CreateTemporaryFile(&test_file_path);
    base::FilePath new_file_path = test_file_path.AddExtension(extension);
    EXPECT_TRUE(base::ReplaceFile(test_file_path, new_file_path, nullptr));
    return new_file_path;
  }

  std::string InstallFileHandlingPWA() {
    GURL url = GetSecureAppURL();

    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->app_url = url;
    web_app_info->scope = url.GetWithoutFilename();
    web_app_info->title = base::ASCIIToUTF16("A Hosted App");

    blink::Manifest::FileHandler entry1;
    entry1.action = GetTextFileHandlerActionURL();
    entry1.name = base::ASCIIToUTF16("text");
    entry1.accept[base::ASCIIToUTF16("text/*")].push_back(
        base::ASCIIToUTF16(".txt"));
    web_app_info->file_handlers.push_back(std::move(entry1));

    blink::Manifest::FileHandler entry2;
    entry2.action = GetCSVFileHandlerActionURL();
    entry2.name = base::ASCIIToUTF16("csv");
    entry2.accept[base::ASCIIToUTF16("application/csv")].push_back(
        base::ASCIIToUTF16(".csv"));
    web_app_info->file_handlers.push_back(std::move(entry2));

    return WebAppControllerBrowserTest::InstallWebApp(std::move(web_app_info));
  }

  content::WebContents* LaunchWithFiles(
      const std::string& app_id,
      const GURL& expected_launch_url,
      const std::vector<base::FilePath>& files,
      const apps::mojom::LaunchContainer launch_container =
          apps::mojom::LaunchContainer::kLaunchContainerWindow) {
    apps::AppLaunchParams params(
        app_id, launch_container, WindowOpenDisposition::NEW_WINDOW,
        apps::mojom::AppLaunchSource::kSourceFileHandler);
    params.launch_files = files;

    content::TestNavigationObserver navigation_observer(expected_launch_url);
    navigation_observer.StartWatchingNewWebContents();

    content::WebContents* web_contents =
        apps::LaunchService::Get(profile())->OpenApplication(params);

    navigation_observer.Wait();

    // Attach the launchParams to the window so we can inspect them easily.
    auto result = content::EvalJs(web_contents,
                                  "launchQueue.setConsumer(launchParams => {"
                                  "  window.launchParams = launchParams;"
                                  "});");

    return web_contents;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(WebAppFileHandlingBrowserTest,
                       LaunchConsumerIsNotTriggeredWithNoFiles) {
  ASSERT_TRUE(https_server()->Start());

  const std::string app_id = InstallFileHandlingPWA();
  content::WebContents* web_contents =
      LaunchWithFiles(app_id, GetSecureAppURL(), {});
  EXPECT_EQ(false, content::EvalJs(web_contents, "!!window.launchParams"));
}

IN_PROC_BROWSER_TEST_P(WebAppFileHandlingBrowserTest,
                       PWAsCanReceiveFileLaunchParams) {
  ASSERT_TRUE(https_server()->Start());

  const std::string app_id = InstallFileHandlingPWA();
  base::FilePath test_file_path = NewTestFilePath(FILE_PATH_LITERAL("txt"));
  content::WebContents* web_contents =
      LaunchWithFiles(app_id, GetTextFileHandlerActionURL(), {test_file_path});

  EXPECT_EQ(1,
            content::EvalJs(web_contents, "window.launchParams.files.length"));
  EXPECT_EQ(test_file_path.BaseName().value(),
            content::EvalJs(web_contents, "window.launchParams.files[0].name"));
}

IN_PROC_BROWSER_TEST_P(WebAppFileHandlingBrowserTest,
                       PWAsCanReceiveFileLaunchParamsInTab) {
  ASSERT_TRUE(https_server()->Start());

  const std::string app_id = InstallFileHandlingPWA();
  base::FilePath test_file_path = NewTestFilePath(FILE_PATH_LITERAL("txt"));
  content::WebContents* web_contents =
      LaunchWithFiles(app_id, GetTextFileHandlerActionURL(), {test_file_path},
                      apps::mojom::LaunchContainer::kLaunchContainerTab);

  EXPECT_EQ(1,
            content::EvalJs(web_contents, "window.launchParams.files.length"));
  EXPECT_EQ(test_file_path.BaseName().value(),
            content::EvalJs(web_contents, "window.launchParams.files[0].name"));
}

IN_PROC_BROWSER_TEST_P(WebAppFileHandlingBrowserTest,
                       PWAsDispatchOnCorrectFileHandlingURL) {
  ASSERT_TRUE(https_server()->Start());

  const std::string app_id = InstallFileHandlingPWA();

  // Test that file handler dispatches correct URL based on file extension.
  LaunchWithFiles(app_id, GetSecureAppURL(), {});
  LaunchWithFiles(app_id, GetTextFileHandlerActionURL(),
                  {NewTestFilePath(FILE_PATH_LITERAL("txt"))});
  LaunchWithFiles(app_id, GetCSVFileHandlerActionURL(),
                  {NewTestFilePath(FILE_PATH_LITERAL("csv"))});

  // Test as above in a tab.
  LaunchWithFiles(app_id, GetSecureAppURL(), {},
                  apps::mojom::LaunchContainer::kLaunchContainerTab);
  LaunchWithFiles(app_id, GetTextFileHandlerActionURL(),
                  {NewTestFilePath(FILE_PATH_LITERAL("txt"))},
                  apps::mojom::LaunchContainer::kLaunchContainerTab);
  LaunchWithFiles(app_id, GetCSVFileHandlerActionURL(),
                  {NewTestFilePath(FILE_PATH_LITERAL("csv"))},
                  apps::mojom::LaunchContainer::kLaunchContainerTab);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebAppFileHandlingBrowserTest,
    ::testing::Values(
        web_app::ControllerType::kHostedAppController,
        web_app::ControllerType::kUnifiedControllerWithBookmarkApp,
        web_app::ControllerType::kUnifiedControllerWithWebApp),
    web_app::ControllerTypeParamToString);
