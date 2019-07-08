// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_manager.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/test/test_app_registrar.h"
#include "chrome/browser/web_applications/test/test_data_retriever.h"
#include "chrome/browser/web_applications/test/test_install_finalizer.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

namespace {

const GURL kIconUrl{"https://example.com/app.ico"};

}  // namespace

class WebAppInstallManagerTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();

    registrar_ = std::make_unique<TestAppRegistrar>();

    install_finalizer_ = std::make_unique<TestInstallFinalizer>();

    install_manager_ = std::make_unique<WebAppInstallManager>(profile());
    install_manager_->SetSubsystems(registrar_.get(), install_finalizer_.get());

    auto test_url_loader = std::make_unique<TestWebAppUrlLoader>();

    test_url_loader->SetNextLoadUrlResult(GURL("about:blank"),
                                          WebAppUrlLoader::Result::kUrlLoaded);

    test_url_loader_ = test_url_loader.get();
    install_manager_->SetUrlLoaderForTesting(std::move(test_url_loader));
  }

  WebAppInstallManager& install_manager() { return *install_manager_; }
  TestInstallFinalizer& finalizer() { return *install_finalizer_; }
  TestWebAppUrlLoader& url_loader() { return *test_url_loader_; }

  std::unique_ptr<WebApplicationInfo> CreateWebAppInfo(const GURL& url) {
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->app_url = url;
    WebApplicationInfo::IconInfo icon_info;
    icon_info.url = kIconUrl;
    icon_info.width = icon_size::k256;
    web_app_info->icons.push_back(std::move(icon_info));
    return web_app_info;
  }

  void DestroyManagers() {
    // The reverse order of creation:
    install_manager_.reset();
    install_finalizer_.reset();
    registrar_.reset();

    test_url_loader_ = nullptr;
  }

 private:
  std::unique_ptr<TestAppRegistrar> registrar_;
  std::unique_ptr<WebAppInstallManager> install_manager_;
  std::unique_ptr<TestInstallFinalizer> install_finalizer_;

  // A weak ptr. The original is owned by install_manager_.
  TestWebAppUrlLoader* test_url_loader_ = nullptr;
};

TEST_F(WebAppInstallManagerTest,
       InstallOrUpdateWebAppFromSync_TwoConcurrentInstallsAreRunInOrder) {
  const GURL url1{"https://example.com/path"};
  const AppId app1_id = GenerateAppIdFromURL(url1);

  const GURL url2{"https://example.org/path"};
  const AppId app2_id = GenerateAppIdFromURL(url2);

  // 1 InstallTask == 1 DataRetriever, their lifetime matches.
  base::flat_set<TestDataRetriever*> task_data_retrievers;

  base::RunLoop app1_installed_run_loop;
  base::RunLoop app2_installed_run_loop;

  enum class Event {
    Task1_Queued,
    Task2_Queued,
    Task1_Started,
    Task1_Completed,
    App1_CallbackCalled,
    Task2_Started,
    Task2_Completed,
    App2_CallbackCalled,
  };

  std::vector<Event> event_order;

  int task_index = 0;

  install_manager().SetDataRetrieverFactoryForTesting(
      base::BindLambdaForTesting([&]() {
        auto data_retriever = std::make_unique<TestDataRetriever>();
        task_index++;

        TestDataRetriever* data_retriever_ptr = data_retriever.get();
        task_data_retrievers.insert(data_retriever_ptr);

        event_order.push_back(task_index == 1 ? Event::Task1_Queued
                                              : Event::Task2_Queued);

        // Every InstallTask starts with WebAppDataRetriever::GetIcons step.
        data_retriever->SetGetIconsDelegate(base::BindLambdaForTesting(
            [&, task_index](content::WebContents* web_contents,
                            const std::vector<GURL>& icon_urls,
                            bool skip_page_favicons) {
              event_order.push_back(task_index == 1 ? Event::Task1_Started
                                                    : Event::Task2_Started);
              IconsMap icons_map;
              AddIconToIconsMap(kIconUrl, icon_size::k256, SK_ColorBLUE,
                                &icons_map);
              return icons_map;
            }));

        // Every InstallTask ends with WebAppDataRetriever destructor.
        data_retriever->SetDestructionCallback(
            base::BindLambdaForTesting([&task_data_retrievers, &event_order,
                                        data_retriever_ptr, task_index]() {
              event_order.push_back(task_index == 1 ? Event::Task1_Completed
                                                    : Event::Task2_Completed);
              task_data_retrievers.erase(data_retriever_ptr);
            }));

        return std::unique_ptr<WebAppDataRetriever>(std::move(data_retriever));
      }));

  EXPECT_FALSE(install_manager().has_web_contents_for_testing());

  // Enqueue a request to install the 1st app.
  install_manager().InstallOrUpdateWebAppFromSync(
      app1_id, CreateWebAppInfo(url1),
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kSuccess, code);
            EXPECT_EQ(app1_id, installed_app_id);
            event_order.push_back(Event::App1_CallbackCalled);
            app1_installed_run_loop.Quit();
          }));

  EXPECT_TRUE(install_manager().has_web_contents_for_testing());
  EXPECT_EQ(0u, finalizer().finalize_options_list().size());
  EXPECT_EQ(1u, task_data_retrievers.size());

  // Immediately enqueue a request to install the 2nd app, WebContents is not
  // ready.
  install_manager().InstallOrUpdateWebAppFromSync(
      app2_id, CreateWebAppInfo(url2),
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kSuccess, code);
            EXPECT_EQ(app2_id, installed_app_id);
            event_order.push_back(Event::App2_CallbackCalled);
            app2_installed_run_loop.Quit();
          }));

  EXPECT_TRUE(install_manager().has_web_contents_for_testing());
  EXPECT_EQ(2u, task_data_retrievers.size());
  EXPECT_EQ(0u, finalizer().finalize_options_list().size());

  // Wait for the 1st app installed.
  app1_installed_run_loop.Run();
  EXPECT_TRUE(install_manager().has_web_contents_for_testing());
  EXPECT_EQ(1u, task_data_retrievers.size());
  EXPECT_EQ(1u, finalizer().finalize_options_list().size());

  // Wait for the 2nd app installed.
  app2_installed_run_loop.Run();
  EXPECT_FALSE(install_manager().has_web_contents_for_testing());
  EXPECT_EQ(0u, task_data_retrievers.size());
  EXPECT_EQ(2u, finalizer().finalize_options_list().size());

  const std::vector<Event> expected_event_order{
      Event::Task1_Queued,    Event::Task2_Queued,        Event::Task1_Started,
      Event::Task1_Completed, Event::App1_CallbackCalled, Event::Task2_Started,
      Event::Task2_Completed, Event::App2_CallbackCalled,
  };

  EXPECT_EQ(expected_event_order, event_order);
}

TEST_F(WebAppInstallManagerTest,
       InstallOrUpdateWebAppFromSync_InstallManagerShutdown) {
  const GURL app_url("https://example.com/path");
  const AppId app_id = GenerateAppIdFromURL(app_url);
  NavigateAndCommit(app_url);

  base::RunLoop run_loop;

  install_manager().SetDataRetrieverFactoryForTesting(
      base::BindLambdaForTesting([&]() {
        auto data_retriever = std::make_unique<TestDataRetriever>();

        // Every InstallTask starts with WebAppDataRetriever::GetIcons step.
        data_retriever->SetGetIconsDelegate(base::BindLambdaForTesting(
            [&](content::WebContents* web_contents,
                const std::vector<GURL>& icon_urls, bool skip_page_favicons) {
              run_loop.Quit();

              IconsMap icons_map;
              AddIconToIconsMap(kIconUrl, icon_size::k256, SK_ColorBLUE,
                                &icons_map);
              return icons_map;
            }));

        return std::unique_ptr<WebAppDataRetriever>(std::move(data_retriever));
      }));

  install_manager().InstallOrUpdateWebAppFromSync(
      app_id, CreateWebAppInfo(app_url),
      base::BindLambdaForTesting(
          [](const web_app::AppId& installed_app_id,
             web_app::InstallResultCode code) { NOTREACHED(); }));
  EXPECT_TRUE(install_manager().has_web_contents_for_testing());

  // Wait for the task started.
  run_loop.Run();
  EXPECT_TRUE(install_manager().has_web_contents_for_testing());

  // Destroy InstallManager: Call Shutdown as if Profile gets destroyed.
  install_manager().Shutdown();
  EXPECT_FALSE(install_manager().has_web_contents_for_testing());

  // Delete InstallManager object.
  DestroyManagers();
}

TEST_F(WebAppInstallManagerTest,
       InstallOrUpdateWebAppFromSync_InstallAfterShutdown) {
  const GURL app1_url("https://example.com/path");
  const AppId app1_id = GenerateAppIdFromURL(app1_url);

  install_manager().InstallOrUpdateWebAppFromSync(
      app1_id, CreateWebAppInfo(app1_url),
      base::BindLambdaForTesting(
          [&](const web_app::AppId& installed_app_id,
              web_app::InstallResultCode code) { NOTREACHED(); }));
  EXPECT_TRUE(install_manager().has_web_contents_for_testing());

  // Destroy InstallManager: Call Shutdown as if Profile gets destroyed.
  install_manager().Shutdown();
  EXPECT_FALSE(install_manager().has_web_contents_for_testing());

  const GURL app2_url("https://example.org/path");
  const AppId app2_id = GenerateAppIdFromURL(app2_url);

  install_manager().InstallOrUpdateWebAppFromSync(
      app2_id, CreateWebAppInfo(app2_url),
      base::BindLambdaForTesting(
          [&](const web_app::AppId& installed_app_id,
              web_app::InstallResultCode code) { NOTREACHED(); }));
  EXPECT_FALSE(install_manager().has_web_contents_for_testing());

  // Delete InstallManager object.
  DestroyManagers();
}

TEST_F(WebAppInstallManagerTest,
       InstallOrUpdateWebAppFromSync_CanSkipAppUpdateForSync) {
  const GURL app_url("https://example.com/path");
  const AppId app_id = GenerateAppIdFromURL(app_url);
  NavigateAndCommit(app_url);

  finalizer().SetNextCanSkipAppUpdateForSync(true);

  base::RunLoop run_loop;
  install_manager().InstallOrUpdateWebAppFromSync(
      app_id, CreateWebAppInfo(app_url),
      base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                     web_app::InstallResultCode code) {
        EXPECT_EQ(InstallResultCode::kAlreadyInstalled, code);
        EXPECT_EQ(app_id, installed_app_id);
        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace web_app
