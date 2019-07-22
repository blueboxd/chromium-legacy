// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/bookmark_apps/bookmark_app_install_manager.h"

#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/extensions/bookmark_app_helper.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/installable/installable_manager.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/web_applications/components/external_install_options.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/test/test_data_retriever.h"
#include "chrome/browser/web_applications/test/test_install_finalizer.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class BookmarkAppInstallManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    install_finalizer_ = std::make_unique<web_app::TestInstallFinalizer>();
    install_manager_ = std::make_unique<BookmarkAppInstallManager>(profile());
    install_manager_->SetSubsystems(nullptr, install_finalizer_.get());

    extensions::TestExtensionSystem* test_extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile()));

    test_extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(),
        profile()->GetPath().Append(FILE_PATH_LITERAL("Extensions")),
        /*autoupdate_enabled=*/false);

    InstallableManager::CreateForWebContents(web_contents());
    // Required by InstallableManager.
    // Causes eligibility check to return NOT_FROM_SECURE_ORIGIN for GetData.
    SecurityStateTabHelper::CreateForWebContents(web_contents());
  }

  void TearDown() override {
    // Do not destroy profile and its scoped temp dir: ensure that
    // ConvertWebAppOnFileThread finishes.
    RunExtensionServiceTaskRunner();

    ChromeRenderViewHostTestHarness::TearDown();
  }

  // Waits for a round trip between file task runner used by the profile's
  // extension service and the main thread - used to ensure that all pending
  // file runner task finish,
  void RunExtensionServiceTaskRunner() {
    base::RunLoop run_loop;
    GetExtensionFileTaskRunner()->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                                   run_loop.QuitClosure());
    run_loop.Run();
  }

 protected:
  std::unique_ptr<web_app::TestInstallFinalizer> install_finalizer_;
  std::unique_ptr<BookmarkAppInstallManager> install_manager_;
};

TEST_F(BookmarkAppInstallManagerTest, FromManifest_WebContentsDestroyed) {
  NavigateAndCommit(GURL("https://example.com/path"));

  base::RunLoop run_loop;
  bool callback_called = false;

  install_manager_->InstallWebAppFromManifest(
      web_contents(), WebappInstallSource::MENU_BROWSER_TAB, base::DoNothing(),
      base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                     web_app::InstallResultCode code) {
        EXPECT_EQ(web_app::InstallResultCode::kWebContentsDestroyed, code);
        EXPECT_EQ(web_app::AppId(), installed_app_id);
        callback_called = true;
        run_loop.Quit();
      }));

  // Destroy WebContents.
  DeleteContents();
  EXPECT_EQ(nullptr, web_contents());

  run_loop.Run();

  EXPECT_TRUE(callback_called);
}

TEST_F(BookmarkAppInstallManagerTest, FromInfo_InstallManagerDestroyed) {
  const GURL url("https://example.com/path");
  NavigateAndCommit(url);

  auto web_application_info = std::make_unique<WebApplicationInfo>();
  web_application_info->app_url = url;

  base::RunLoop run_loop;
  bool callback_called = false;

  install_manager_->InstallWebAppFromInfo(
      std::move(web_application_info),
      /*no_network_install=*/true, WebappInstallSource::ARC,
      base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                     web_app::InstallResultCode code) {
        EXPECT_EQ(web_app::InstallResultCode::kInstallManagerDestroyed, code);
        EXPECT_EQ(web_app::AppId(), installed_app_id);
        callback_called = true;
        run_loop.Quit();
      }));

  // Destroy InstallManager: Call Shutdown as if Profile gets destroyed.
  install_manager_->Shutdown();
  run_loop.Run();

  // Delete InstallManager object.
  install_manager_.reset();
  EXPECT_TRUE(callback_called);
}

TEST_F(BookmarkAppInstallManagerTest, WithOptions_WebContentsDestroyed) {
  const GURL app_url("https://example.com/path");
  NavigateAndCommit(app_url);

  web_app::ExternalInstallOptions install_options(
      app_url, web_app::LaunchContainer::kWindow,
      web_app::ExternalInstallSource::kExternalPolicy);

  base::RunLoop run_loop;
  bool callback_called = false;

  install_manager_->InstallWebAppWithOptions(
      web_contents(), install_options,
      base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                     web_app::InstallResultCode code) {
        EXPECT_EQ(web_app::InstallResultCode::kWebContentsDestroyed, code);
        EXPECT_EQ(web_app::AppId(), installed_app_id);
        callback_called = true;
        run_loop.Quit();
      }));
  EXPECT_FALSE(callback_called);

  // Destroy WebContents.
  DeleteContents();
  EXPECT_EQ(nullptr, web_contents());

  run_loop.Run();

  EXPECT_TRUE(callback_called);
}

TEST_F(BookmarkAppInstallManagerTest,
       WithOptions_WebContentsDestroyedAfterDataRetrieval) {
  const GURL app_url("https://example.com/path");
  NavigateAndCommit(app_url);

  web_app::ExternalInstallOptions install_options(
      app_url, web_app::LaunchContainer::kWindow,
      web_app::ExternalInstallSource::kExternalPolicy);

  base::RunLoop retrieval_run_loop;
  bool data_retrieval_passed = false;

  install_manager_->SetDataRetrieverFactoryForTesting(
      base::BindLambdaForTesting([&]() {
        auto info = std::make_unique<WebApplicationInfo>();
        info->app_url = app_url;
        auto data_retriever = std::make_unique<web_app::TestDataRetriever>();
        data_retriever->SetRendererWebApplicationInfo(std::move(info));

        return std::unique_ptr<web_app::WebAppDataRetriever>(
            std::move(data_retriever));
      }));

  install_manager_->SetBookmarkAppHelperFactoryForTesting(
      base::BindLambdaForTesting(
          [&](Profile* profile,
              std::unique_ptr<WebApplicationInfo> web_app_info,
              content::WebContents* web_contents,
              WebappInstallSource install_source) {
            data_retrieval_passed = true;
            retrieval_run_loop.Quit();
            return std::make_unique<BookmarkAppHelper>(
                profile, std::move(web_app_info), web_contents, install_source);
          }));

  base::RunLoop install_run_loop;
  bool callback_called = false;

  install_manager_->InstallWebAppWithOptions(
      web_contents(), install_options,
      base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                     web_app::InstallResultCode code) {
        EXPECT_EQ(web_app::InstallResultCode::kWebContentsDestroyed, code);
        EXPECT_EQ(web_app::AppId(), installed_app_id);
        callback_called = true;
        install_run_loop.Quit();
      }));
  EXPECT_FALSE(callback_called);

  retrieval_run_loop.Run();
  EXPECT_TRUE(data_retrieval_passed);
  EXPECT_FALSE(callback_called);

  // Destroy WebContents.
  DeleteContents();
  EXPECT_EQ(nullptr, web_contents());

  install_run_loop.Run();
  EXPECT_TRUE(callback_called);
}

TEST_F(BookmarkAppInstallManagerTest, WithOptions_InstallManagerDestroyed) {
  const GURL app_url("https://example.com/path");
  NavigateAndCommit(app_url);

  web_app::ExternalInstallOptions install_options(
      app_url, web_app::LaunchContainer::kWindow,
      web_app::ExternalInstallSource::kExternalPolicy);

  base::RunLoop run_loop;
  bool callback_called = false;

  install_manager_->InstallWebAppWithOptions(
      web_contents(), install_options,
      base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                     web_app::InstallResultCode code) {
        EXPECT_EQ(web_app::InstallResultCode::kInstallManagerDestroyed, code);
        EXPECT_EQ(web_app::AppId(), installed_app_id);
        callback_called = true;
        run_loop.Quit();
      }));
  EXPECT_FALSE(callback_called);

  // Destroy InstallManager: Call Shutdown as if Profile gets destroyed.
  install_manager_->Shutdown();
  run_loop.Run();

  // Delete InstallManager object.
  install_manager_.reset();

  EXPECT_TRUE(callback_called);
}

TEST_F(BookmarkAppInstallManagerTest,
       WithOptions_InstallManagerDestroyedAfterDataRetrieval) {
  const GURL app_url("https://example.com/path");
  NavigateAndCommit(app_url);

  web_app::ExternalInstallOptions install_options(
      app_url, web_app::LaunchContainer::kWindow,
      web_app::ExternalInstallSource::kExternalPolicy);

  base::RunLoop retrieval_run_loop;
  bool data_retrieval_passed = false;

  install_manager_->SetDataRetrieverFactoryForTesting(
      base::BindLambdaForTesting([&]() {
        auto info = std::make_unique<WebApplicationInfo>();
        info->app_url = app_url;
        auto data_retriever = std::make_unique<web_app::TestDataRetriever>();
        data_retriever->SetRendererWebApplicationInfo(std::move(info));

        return std::unique_ptr<web_app::WebAppDataRetriever>(
            std::move(data_retriever));
      }));

  install_manager_->SetBookmarkAppHelperFactoryForTesting(
      base::BindLambdaForTesting(
          [&](Profile* profile,
              std::unique_ptr<WebApplicationInfo> web_app_info,
              content::WebContents* web_contents,
              WebappInstallSource install_source) {
            data_retrieval_passed = true;
            retrieval_run_loop.Quit();
            return std::make_unique<BookmarkAppHelper>(
                profile, std::move(web_app_info), web_contents, install_source);
          }));

  base::RunLoop install_run_loop;
  bool callback_called = false;

  install_manager_->InstallWebAppWithOptions(
      web_contents(), install_options,
      base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                     web_app::InstallResultCode code) {
        EXPECT_EQ(web_app::InstallResultCode::kInstallManagerDestroyed, code);
        EXPECT_EQ(web_app::AppId(), installed_app_id);
        callback_called = true;
        install_run_loop.Quit();
      }));
  EXPECT_FALSE(callback_called);

  retrieval_run_loop.Run();
  EXPECT_TRUE(data_retrieval_passed);
  EXPECT_FALSE(callback_called);

  // Destroy InstallManager: Call Shutdown as if Profile gets destroyed.
  install_manager_->Shutdown();
  install_run_loop.Run();

  // Delete InstallManager object.
  install_manager_.reset();
  EXPECT_TRUE(callback_called);
}

}  // namespace extensions
