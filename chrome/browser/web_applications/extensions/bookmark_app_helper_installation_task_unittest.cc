// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_installation_task.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/extensions/bookmark_app_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/installable/installable_data.h"
#include "chrome/browser/web_applications/bookmark_apps/bookmark_app_install_manager.h"
#include "chrome/browser/web_applications/bookmark_apps/test_web_app_provider.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_data_retriever.h"
#include "chrome/browser/web_applications/test/test_app_registrar.h"
#include "chrome/browser/web_applications/test/test_data_retriever.h"
#include "chrome/browser/web_applications/test/test_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace extensions {

using Result = BookmarkAppInstallationTask::Result;

namespace {

const char kWebAppTitle[] = "Foo Title";
const GURL kWebAppUrl("https://foo.example");

// TODO(ortuno): Move this to ExternallyInstalledWebAppPrefs or replace with a
// method in ExternallyInstalledWebAppPrefs once there is one.
bool IsPlaceholderApp(Profile* profile, const GURL& url) {
  const base::Value* map =
      profile->GetPrefs()->GetDictionary(prefs::kWebAppsExtensionIDs);

  const base::Value* entry = map->FindKey(url.spec());

  return entry->FindBoolKey("is_placeholder").value();
}

class TestBookmarkAppHelper : public BookmarkAppHelper {
 public:
  using BookmarkAppHelper::BookmarkAppHelper;
  ~TestBookmarkAppHelper() override {}

  void CompleteInstallation() {
    CompleteInstallableCheck();
    content::RunAllTasksUntilIdle();
    CompleteIconDownload();
    content::RunAllTasksUntilIdle();
  }

  void CompleteInstallableCheck() {
    blink::Manifest manifest;
    InstallableData data = {
        {NO_MANIFEST},
        GURL::EmptyGURL(),
        &manifest,
        GURL::EmptyGURL(),
        nullptr,
        false,
        GURL::EmptyGURL(),
        nullptr,
        false,
        false,
    };
    BookmarkAppHelper::OnDidPerformInstallableCheck(data);
  }

  void CompleteIconDownload() {
    BookmarkAppHelper::OnIconsDownloaded(
        true, std::map<GURL, std::vector<SkBitmap>>());
  }

  void FailIconDownload() {
    BookmarkAppHelper::OnIconsDownloaded(
        false, std::map<GURL, std::vector<SkBitmap>>());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestBookmarkAppHelper);
};

web_app::AppId GetAppIdForUrl(const GURL& url) {
  return web_app::TestInstallFinalizer::GetAppIdForUrl(url);
}

class TestBookmarkAppInstallFinalizer : public web_app::InstallFinalizer {
 public:
  explicit TestBookmarkAppInstallFinalizer(web_app::TestAppRegistrar* registrar)
      : registrar_(registrar) {}
  ~TestBookmarkAppInstallFinalizer() override = default;

  void SetNextFinalizeInstallResult(const GURL& url,
                                    web_app::InstallResultCode code) {
    DCHECK(!base::Contains(next_finalize_install_results_, url));

    web_app::AppId app_id;
    if (code == web_app::InstallResultCode::kSuccess) {
      app_id = GetAppIdForUrl(url);
    }
    next_finalize_install_results_[url] = {app_id, code};
  }

  void SetNextUninstallExternalWebAppResult(const GURL& app_url,
                                            bool uninstalled) {
    DCHECK(!base::Contains(next_uninstall_external_web_app_results_, app_url));

    next_uninstall_external_web_app_results_[app_url] = {
        GetAppIdForUrl(app_url), uninstalled};
  }

  void SetNextCreateOsShortcutsResult(const web_app::AppId& app_id,
                                      bool shortcut_created) {
    DCHECK(!base::Contains(next_create_os_shortcuts_results_, app_id));
    next_create_os_shortcuts_results_[app_id] = shortcut_created;
  }

  const std::vector<WebApplicationInfo>& web_app_info_list() {
    return web_app_info_list_;
  }

  const std::vector<FinalizeOptions>& finalize_options_list() {
    return finalize_options_list_;
  }

  const std::vector<GURL>& uninstall_external_web_app_urls() const {
    return uninstall_external_web_app_urls_;
  }

  size_t num_create_os_shortcuts_calls() {
    return num_create_os_shortcuts_calls_;
  }

  // InstallFinalizer
  void FinalizeInstall(const WebApplicationInfo& web_app_info,
                       const FinalizeOptions& options,
                       InstallFinalizedCallback callback) override {
    DCHECK(
        base::Contains(next_finalize_install_results_, web_app_info.app_url));

    web_app_info_list_.push_back(web_app_info);
    finalize_options_list_.push_back(options);

    web_app::AppId app_id;
    web_app::InstallResultCode code;
    std::tie(app_id, code) =
        next_finalize_install_results_[web_app_info.app_url];
    next_finalize_install_results_.erase(web_app_info.app_url);
    const GURL& url = web_app_info.app_url;

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindLambdaForTesting([&, app_id, url, code,
                                    callback = std::move(callback)]() mutable {
          registrar_->AddExternalApp(
              app_id, {url, web_app::ExternalInstallSource::kInternalDefault});
          std::move(callback).Run(app_id, code);
        }));
  }

  void UninstallExternalWebApp(
      const GURL& app_url,
      UninstallExternalWebAppCallback callback) override {
    DCHECK(base::Contains(next_uninstall_external_web_app_results_, app_url));
    uninstall_external_web_app_urls_.push_back(app_url);

    web_app::AppId app_id;
    bool uninstalled;
    std::tie(app_id, uninstalled) =
        next_uninstall_external_web_app_results_[app_url];
    next_uninstall_external_web_app_results_.erase(app_url);

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindLambdaForTesting(
            [&, app_id, uninstalled, callback = std::move(callback)]() mutable {
              if (uninstalled)
                registrar_->RemoveExternalApp(app_id);
              std::move(callback).Run(uninstalled);
            }));
  }

  bool CanCreateOsShortcuts() const override { return true; }

  void CreateOsShortcuts(const web_app::AppId& app_id,
                         bool add_to_desktop,
                         CreateOsShortcutsCallback callback) override {
    DCHECK(base::Contains(next_create_os_shortcuts_results_, app_id));
    ++num_create_os_shortcuts_calls_;
    bool shortcut_created = next_create_os_shortcuts_results_[app_id];
    next_create_os_shortcuts_results_.erase(app_id);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), shortcut_created));
  }

  bool CanPinAppToShelf() const override { return true; }

  void PinAppToShelf(const web_app::AppId& app_id) override {
    ++num_pin_app_to_shelf_calls_;
  }

  bool CanReparentTab(const web_app::AppId& app_id,
                      bool shortcut_created) const override {
    NOTIMPLEMENTED();
    return true;
  }

  void ReparentTab(const web_app::AppId& app_id,
                   content::WebContents* web_contents) override {
    NOTIMPLEMENTED();
  }

  bool CanRevealAppShim() const override {
    NOTIMPLEMENTED();
    return true;
  }

  void RevealAppShim(const web_app::AppId& app_id) override {
    NOTIMPLEMENTED();
  }

  bool CanSkipAppUpdateForSync(
      const web_app::AppId& app_id,
      const WebApplicationInfo& web_app_info) const override {
    NOTIMPLEMENTED();
    return true;
  }

 private:
  web_app::TestAppRegistrar* registrar_ = nullptr;

  std::vector<WebApplicationInfo> web_app_info_list_;
  std::vector<FinalizeOptions> finalize_options_list_;
  std::vector<GURL> uninstall_external_web_app_urls_;

  size_t num_create_os_shortcuts_calls_ = 0;
  size_t num_pin_app_to_shelf_calls_ = 0;

  std::map<GURL, std::pair<web_app::AppId, web_app::InstallResultCode>>
      next_finalize_install_results_;

  // Maps app URLs to the id of the app that would have been installed for that
  // url and the result of trying to uninstall it.
  std::map<GURL, std::pair<web_app::AppId, bool>>
      next_uninstall_external_web_app_results_;

  std::map<web_app::AppId, bool> next_create_os_shortcuts_results_;

  DISALLOW_COPY_AND_ASSIGN(TestBookmarkAppInstallFinalizer);
};

}  // namespace

// Deprecated tests. TODO(crbug.com/915043): Erase this .cc file.
class BookmarkAppHelperInstallationTaskTest
    : public ChromeRenderViewHostTestHarness {
 public:
  BookmarkAppHelperInstallationTaskTest() {
    scoped_feature_list_.InitWithFeatures(
        {}, {features::kDesktopPWAsUnifiedInstall});
  }

  ~BookmarkAppHelperInstallationTaskTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    registrar_ = std::make_unique<web_app::TestAppRegistrar>();
    install_finalizer_ =
        std::make_unique<TestBookmarkAppInstallFinalizer>(registrar_.get());

    // CrxInstaller in BookmarkAppInstaller needs an ExtensionService, so
    // create one for the profile.
    TestExtensionSystem* test_system =
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile()));
    test_system->CreateExtensionService(base::CommandLine::ForCurrentProcess(),
                                        profile()->GetPath(),
                                        false /* autoupdate_enabled */);
    SetUpTestingFactories();
  }

 protected:
  TestBookmarkAppHelper& test_helper() {
    DCHECK(test_helper_);
    return *test_helper_;
  }

  web_app::TestAppRegistrar* registrar() { return registrar_.get(); }

  TestBookmarkAppInstallFinalizer* install_finalizer() {
    return install_finalizer_.get();
  }

 private:
  void SetUpTestingFactories() {
    auto* provider = web_app::TestWebAppProvider::Get(profile());
    provider->Start();

    BookmarkAppInstallManager* install_manager =
        static_cast<BookmarkAppInstallManager*>(&provider->install_manager());

    install_manager->SetBookmarkAppHelperFactoryForTesting(
        base::BindLambdaForTesting(
            [this](Profile* profile,
                   std::unique_ptr<WebApplicationInfo> web_app_info,
                   content::WebContents* web_contents,
                   WebappInstallSource install_source) {
              auto test_helper = std::make_unique<TestBookmarkAppHelper>(
                  profile, std::move(web_app_info), web_contents,
                  install_source);
              test_helper_ = test_helper.get();

              return std::unique_ptr<BookmarkAppHelper>(std::move(test_helper));
            }));

    install_manager->SetDataRetrieverFactoryForTesting(
        base::BindLambdaForTesting([]() {
          auto info = std::make_unique<WebApplicationInfo>();
          info->app_url = kWebAppUrl;
          info->title = base::UTF8ToUTF16(kWebAppTitle);
          auto data_retriever = std::make_unique<web_app::TestDataRetriever>();
          data_retriever->SetRendererWebApplicationInfo(std::move(info));

          return std::unique_ptr<web_app::WebAppDataRetriever>(
              std::move(data_retriever));
        }));
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<web_app::TestAppRegistrar> registrar_;
  std::unique_ptr<TestBookmarkAppInstallFinalizer> install_finalizer_;
  TestBookmarkAppHelper* test_helper_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(BookmarkAppHelperInstallationTaskTest);
};

TEST_F(BookmarkAppHelperInstallationTaskTest,
       WebAppOrShortcutFromContents_InstallationSucceeds) {
  auto task = std::make_unique<BookmarkAppInstallationTask>(
      profile(), registrar(), install_finalizer(),
      web_app::ExternalInstallOptions(
          kWebAppUrl, web_app::LaunchContainer::kDefault,
          web_app::ExternalInstallSource::kInternalDefault));

  bool callback_called = false;
  task->Install(
      web_contents(), web_app::WebAppUrlLoader::Result::kUrlLoaded,
      base::BindLambdaForTesting(
          [&](BookmarkAppInstallationTask::Result result) {
            base::Optional<web_app::AppId> id =
                web_app::ExternallyInstalledWebAppPrefs(profile()->GetPrefs())
                    .LookupAppId(kWebAppUrl);

            EXPECT_EQ(web_app::InstallResultCode::kSuccess, result.code);
            EXPECT_TRUE(result.app_id.has_value());

            EXPECT_FALSE(IsPlaceholderApp(profile(), kWebAppUrl));

            EXPECT_EQ(result.app_id.value(), id.value());

            EXPECT_TRUE(test_helper().add_to_quick_launch_bar());
            EXPECT_TRUE(test_helper().add_to_desktop());
            EXPECT_TRUE(test_helper().add_to_quick_launch_bar());
            EXPECT_FALSE(test_helper().forced_launch_type().has_value());
            EXPECT_TRUE(test_helper().is_default_app());
            EXPECT_FALSE(test_helper().is_policy_installed_app());
            callback_called = true;
          }));

  content::RunAllTasksUntilIdle();

  test_helper().CompleteInstallableCheck();
  content::RunAllTasksUntilIdle();

  test_helper().CompleteIconDownload();
  content::RunAllTasksUntilIdle();

  EXPECT_TRUE(callback_called);
}

TEST_F(BookmarkAppHelperInstallationTaskTest,
       WebAppOrShortcutFromContents_InstallationFails) {
  auto task = std::make_unique<BookmarkAppInstallationTask>(
      profile(), registrar(), install_finalizer(),
      web_app::ExternalInstallOptions(
          kWebAppUrl, web_app::LaunchContainer::kWindow,
          web_app::ExternalInstallSource::kInternalDefault));

  bool callback_called = false;
  task->Install(
      web_contents(), web_app::WebAppUrlLoader::Result::kUrlLoaded,
      base::BindLambdaForTesting(
          [&](BookmarkAppInstallationTask::Result result) {
            base::Optional<web_app::AppId> id =
                web_app::ExternallyInstalledWebAppPrefs(profile()->GetPrefs())
                    .LookupAppId(kWebAppUrl);

            EXPECT_NE(web_app::InstallResultCode::kSuccess, result.code);
            EXPECT_FALSE(result.app_id.has_value());

            EXPECT_FALSE(id.has_value());
            callback_called = true;
          }));

  content::RunAllTasksUntilIdle();

  test_helper().CompleteInstallableCheck();
  content::RunAllTasksUntilIdle();

  test_helper().FailIconDownload();
  content::RunAllTasksUntilIdle();

  EXPECT_TRUE(callback_called);
}

TEST_F(BookmarkAppHelperInstallationTaskTest,
       WebAppOrShortcutFromContents_NoDesktopShortcut) {
  web_app::ExternalInstallOptions install_options(
      kWebAppUrl, web_app::LaunchContainer::kWindow,
      web_app::ExternalInstallSource::kInternalDefault);
  install_options.add_to_desktop = false;
  auto task = std::make_unique<BookmarkAppInstallationTask>(
      profile(), registrar(), install_finalizer(), std::move(install_options));

  bool callback_called = false;
  task->Install(web_contents(), web_app::WebAppUrlLoader::Result::kUrlLoaded,
                base::BindLambdaForTesting(
                    [&](BookmarkAppInstallationTask::Result result) {
                      EXPECT_EQ(web_app::InstallResultCode::kSuccess,
                                result.code);
                      EXPECT_TRUE(result.app_id.has_value());

                      EXPECT_TRUE(test_helper().add_to_applications_menu());
                      EXPECT_TRUE(test_helper().add_to_quick_launch_bar());
                      EXPECT_FALSE(test_helper().add_to_desktop());

                      callback_called = true;
                    }));
  content::RunAllTasksUntilIdle();

  test_helper().CompleteInstallation();
  EXPECT_TRUE(callback_called);
}

TEST_F(BookmarkAppHelperInstallationTaskTest,
       WebAppOrShortcutFromContents_NoQuickLaunchBarShortcut) {
  web_app::ExternalInstallOptions install_options(
      kWebAppUrl, web_app::LaunchContainer::kWindow,
      web_app::ExternalInstallSource::kInternalDefault);
  install_options.add_to_quick_launch_bar = false;
  auto task = std::make_unique<BookmarkAppInstallationTask>(
      profile(), registrar(), install_finalizer(), std::move(install_options));

  bool callback_called = false;
  task->Install(web_contents(), web_app::WebAppUrlLoader::Result::kUrlLoaded,
                base::BindLambdaForTesting(
                    [&](BookmarkAppInstallationTask::Result result) {
                      EXPECT_EQ(web_app::InstallResultCode::kSuccess,
                                result.code);
                      EXPECT_TRUE(result.app_id.has_value());

                      EXPECT_TRUE(test_helper().add_to_applications_menu());
                      EXPECT_FALSE(test_helper().add_to_quick_launch_bar());
                      EXPECT_TRUE(test_helper().add_to_desktop());

                      callback_called = true;
                    }));

  content::RunAllTasksUntilIdle();

  test_helper().CompleteInstallation();
  EXPECT_TRUE(callback_called);
}

TEST_F(
    BookmarkAppHelperInstallationTaskTest,
    WebAppOrShortcutFromContents_NoDesktopShortcutAndNoQuickLaunchBarShortcut) {
  web_app::ExternalInstallOptions install_options(
      kWebAppUrl, web_app::LaunchContainer::kWindow,
      web_app::ExternalInstallSource::kInternalDefault);
  install_options.add_to_desktop = false;
  install_options.add_to_quick_launch_bar = false;
  auto task = std::make_unique<BookmarkAppInstallationTask>(
      profile(), registrar(), install_finalizer(), std::move(install_options));

  bool callback_called = false;
  task->Install(web_contents(), web_app::WebAppUrlLoader::Result::kUrlLoaded,
                base::BindLambdaForTesting(
                    [&](BookmarkAppInstallationTask::Result result) {
                      EXPECT_EQ(web_app::InstallResultCode::kSuccess,
                                result.code);
                      EXPECT_TRUE(result.app_id.has_value());

                      EXPECT_TRUE(test_helper().add_to_applications_menu());
                      EXPECT_FALSE(test_helper().add_to_quick_launch_bar());
                      EXPECT_FALSE(test_helper().add_to_desktop());

                      callback_called = true;
                    }));

  content::RunAllTasksUntilIdle();

  test_helper().CompleteInstallation();
  EXPECT_TRUE(callback_called);
}

TEST_F(BookmarkAppHelperInstallationTaskTest,
       WebAppOrShortcutFromContents_ForcedContainerWindow) {
  auto install_options = web_app::ExternalInstallOptions(
      kWebAppUrl, web_app::LaunchContainer::kWindow,
      web_app::ExternalInstallSource::kInternalDefault);
  auto task = std::make_unique<BookmarkAppInstallationTask>(
      profile(), registrar(), install_finalizer(), std::move(install_options));

  bool callback_called = false;
  task->Install(web_contents(), web_app::WebAppUrlLoader::Result::kUrlLoaded,
                base::BindLambdaForTesting(
                    [&](BookmarkAppInstallationTask::Result result) {
                      EXPECT_EQ(web_app::InstallResultCode::kSuccess,
                                result.code);
                      EXPECT_TRUE(result.app_id.has_value());

                      EXPECT_EQ(LAUNCH_TYPE_WINDOW,
                                test_helper().forced_launch_type().value());
                      callback_called = true;
                    }));

  content::RunAllTasksUntilIdle();

  test_helper().CompleteInstallation();
  EXPECT_TRUE(callback_called);
}

TEST_F(BookmarkAppHelperInstallationTaskTest,
       WebAppOrShortcutFromContents_ForcedContainerTab) {
  auto install_options = web_app::ExternalInstallOptions(
      kWebAppUrl, web_app::LaunchContainer::kTab,
      web_app::ExternalInstallSource::kInternalDefault);
  auto task = std::make_unique<BookmarkAppInstallationTask>(
      profile(), registrar(), install_finalizer(), std::move(install_options));

  bool callback_called = false;
  task->Install(web_contents(), web_app::WebAppUrlLoader::Result::kUrlLoaded,
                base::BindLambdaForTesting(
                    [&](BookmarkAppInstallationTask::Result result) {
                      EXPECT_EQ(web_app::InstallResultCode::kSuccess,
                                result.code);
                      EXPECT_TRUE(result.app_id.has_value());

                      EXPECT_EQ(LAUNCH_TYPE_REGULAR,
                                test_helper().forced_launch_type().value());
                      callback_called = true;
                    }));

  content::RunAllTasksUntilIdle();

  test_helper().CompleteInstallation();
  EXPECT_TRUE(callback_called);
}

TEST_F(BookmarkAppHelperInstallationTaskTest,
       WebAppOrShortcutFromContents_DefaultApp) {
  auto install_options = web_app::ExternalInstallOptions(
      kWebAppUrl, web_app::LaunchContainer::kDefault,
      web_app::ExternalInstallSource::kInternalDefault);
  auto task = std::make_unique<BookmarkAppInstallationTask>(
      profile(), registrar(), install_finalizer(), std::move(install_options));

  bool callback_called = false;
  task->Install(web_contents(), web_app::WebAppUrlLoader::Result::kUrlLoaded,
                base::BindLambdaForTesting(
                    [&](BookmarkAppInstallationTask::Result result) {
                      EXPECT_EQ(web_app::InstallResultCode::kSuccess,
                                result.code);
                      EXPECT_TRUE(result.app_id.has_value());

                      EXPECT_TRUE(test_helper().is_default_app());
                      callback_called = true;
                    }));

  content::RunAllTasksUntilIdle();

  test_helper().CompleteInstallation();
  EXPECT_TRUE(callback_called);
}

TEST_F(BookmarkAppHelperInstallationTaskTest,
       WebAppOrShortcutFromContents_AppFromPolicy) {
  auto install_options = web_app::ExternalInstallOptions(
      kWebAppUrl, web_app::LaunchContainer::kDefault,
      web_app::ExternalInstallSource::kExternalPolicy);
  auto task = std::make_unique<BookmarkAppInstallationTask>(
      profile(), registrar(), install_finalizer(), std::move(install_options));

  bool callback_called = false;
  task->Install(web_contents(), web_app::WebAppUrlLoader::Result::kUrlLoaded,
                base::BindLambdaForTesting(
                    [&](BookmarkAppInstallationTask::Result result) {
                      EXPECT_EQ(web_app::InstallResultCode::kSuccess,
                                result.code);
                      EXPECT_TRUE(result.app_id.has_value());

                      EXPECT_TRUE(test_helper().is_policy_installed_app());
                      callback_called = true;
                    }));

  content::RunAllTasksUntilIdle();

  test_helper().CompleteInstallation();
  EXPECT_TRUE(callback_called);
}

TEST_F(BookmarkAppHelperInstallationTaskTest, InstallPlaceholder) {
  web_app::ExternalInstallOptions options(
      kWebAppUrl, web_app::LaunchContainer::kWindow,
      web_app::ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  auto task = std::make_unique<BookmarkAppInstallationTask>(
      profile(), registrar(), install_finalizer(), std::move(options));
  install_finalizer()->SetNextFinalizeInstallResult(
      kWebAppUrl, web_app::InstallResultCode::kSuccess);
  install_finalizer()->SetNextCreateOsShortcutsResult(
      GetAppIdForUrl(kWebAppUrl), true);

  base::RunLoop run_loop;
  task->Install(
      web_contents(), web_app::WebAppUrlLoader::Result::kRedirectedUrlLoaded,
      base::BindLambdaForTesting(
          [&](BookmarkAppInstallationTask::Result result) {
            EXPECT_EQ(web_app::InstallResultCode::kSuccess, result.code);
            EXPECT_TRUE(result.app_id.has_value());

            EXPECT_TRUE(IsPlaceholderApp(profile(), kWebAppUrl));

            EXPECT_EQ(1u, install_finalizer()->num_create_os_shortcuts_calls());
            EXPECT_EQ(1u, install_finalizer()->finalize_options_list().size());
            EXPECT_EQ(
                WebappInstallSource::EXTERNAL_POLICY,
                install_finalizer()->finalize_options_list()[0].install_source);
            const WebApplicationInfo& web_app_info =
                install_finalizer()->web_app_info_list().at(0);

            EXPECT_EQ(base::UTF8ToUTF16(kWebAppUrl.spec()), web_app_info.title);
            EXPECT_EQ(kWebAppUrl, web_app_info.app_url);
            EXPECT_TRUE(web_app_info.open_as_window);
            EXPECT_TRUE(web_app_info.icons.empty());

            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(BookmarkAppHelperInstallationTaskTest, InstallPlaceholderTwice) {
  web_app::ExternalInstallOptions options(
      kWebAppUrl, web_app::LaunchContainer::kWindow,
      web_app::ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  web_app::AppId placeholder_app_id;

  // Install a placeholder app.
  {
    auto task = std::make_unique<BookmarkAppInstallationTask>(
        profile(), registrar(), install_finalizer(), options);
    install_finalizer()->SetNextFinalizeInstallResult(
        kWebAppUrl, web_app::InstallResultCode::kSuccess);
    install_finalizer()->SetNextCreateOsShortcutsResult(
        GetAppIdForUrl(kWebAppUrl), true);

    base::RunLoop run_loop;
    task->Install(
        web_contents(), web_app::WebAppUrlLoader::Result::kRedirectedUrlLoaded,
        base::BindLambdaForTesting(
            [&](BookmarkAppInstallationTask::Result result) {
              EXPECT_EQ(web_app::InstallResultCode::kSuccess, result.code);
              placeholder_app_id = result.app_id.value();

              EXPECT_EQ(1u,
                        install_finalizer()->finalize_options_list().size());
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  // Try to install it again.
  auto task = std::make_unique<BookmarkAppInstallationTask>(
      profile(), registrar(), install_finalizer(), options);
  base::RunLoop run_loop;
  task->Install(
      web_contents(), web_app::WebAppUrlLoader::Result::kRedirectedUrlLoaded,
      base::BindLambdaForTesting(
          [&](BookmarkAppInstallationTask::Result result) {
            EXPECT_EQ(web_app::InstallResultCode::kSuccess, result.code);
            EXPECT_EQ(placeholder_app_id, result.app_id.value());

            // There shouldn't be a second call to the finalizer.
            EXPECT_EQ(1u, install_finalizer()->finalize_options_list().size());

            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(BookmarkAppHelperInstallationTaskTest, ReinstallPlaceholderSucceeds) {
  web_app::ExternalInstallOptions options(
      kWebAppUrl, web_app::LaunchContainer::kWindow,
      web_app::ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  web_app::AppId placeholder_app_id;

  // Install a placeholder app.
  {
    auto task = std::make_unique<BookmarkAppInstallationTask>(
        profile(), registrar(), install_finalizer(), options);
    install_finalizer()->SetNextFinalizeInstallResult(
        kWebAppUrl, web_app::InstallResultCode::kSuccess);
    install_finalizer()->SetNextCreateOsShortcutsResult(
        GetAppIdForUrl(kWebAppUrl), true);

    base::RunLoop run_loop;
    task->Install(
        web_contents(), web_app::WebAppUrlLoader::Result::kRedirectedUrlLoaded,
        base::BindLambdaForTesting(
            [&](BookmarkAppInstallationTask::Result result) {
              EXPECT_EQ(web_app::InstallResultCode::kSuccess, result.code);
              placeholder_app_id = result.app_id.value();

              EXPECT_EQ(1u,
                        install_finalizer()->finalize_options_list().size());
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  // Replace the placeholder with a real app.
  options.reinstall_placeholder = true;
  auto task = std::make_unique<BookmarkAppInstallationTask>(
      profile(), registrar(), install_finalizer(), options);
  install_finalizer()->SetNextUninstallExternalWebAppResult(kWebAppUrl, true);
  base::RunLoop run_loop;
  task->Install(
      web_contents(), web_app::WebAppUrlLoader::Result::kUrlLoaded,
      base::BindLambdaForTesting(
          [&](BookmarkAppInstallationTask::Result result) {
            EXPECT_EQ(web_app::InstallResultCode::kSuccess, result.code);
            EXPECT_TRUE(result.app_id.has_value());
            EXPECT_FALSE(IsPlaceholderApp(profile(), kWebAppUrl));

            EXPECT_EQ(
                1u,
                install_finalizer()->uninstall_external_web_app_urls().size());
            EXPECT_EQ(
                kWebAppUrl,
                install_finalizer()->uninstall_external_web_app_urls().at(0));

            run_loop.Quit();
          }));
  content::RunAllTasksUntilIdle();
  test_helper().CompleteInstallation();
  run_loop.Run();
}

TEST_F(BookmarkAppHelperInstallationTaskTest, ReinstallPlaceholderFails) {
  web_app::ExternalInstallOptions options(
      kWebAppUrl, web_app::LaunchContainer::kWindow,
      web_app::ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  web_app::AppId placeholder_app_id;

  // Install a placeholder app.
  {
    auto task = std::make_unique<BookmarkAppInstallationTask>(
        profile(), registrar(), install_finalizer(), options);
    install_finalizer()->SetNextFinalizeInstallResult(
        kWebAppUrl, web_app::InstallResultCode::kSuccess);
    install_finalizer()->SetNextCreateOsShortcutsResult(
        GetAppIdForUrl(kWebAppUrl), true);

    base::RunLoop run_loop;
    task->Install(
        web_contents(), web_app::WebAppUrlLoader::Result::kRedirectedUrlLoaded,
        base::BindLambdaForTesting(
            [&](BookmarkAppInstallationTask::Result result) {
              EXPECT_EQ(web_app::InstallResultCode::kSuccess, result.code);
              placeholder_app_id = result.app_id.value();

              EXPECT_EQ(1u,
                        install_finalizer()->finalize_options_list().size());

              run_loop.Quit();
            }));
    run_loop.Run();
  }

  // Replace the placeholder with a real app.
  options.reinstall_placeholder = true;
  auto task = std::make_unique<BookmarkAppInstallationTask>(
      profile(), registrar(), install_finalizer(), options);
  install_finalizer()->SetNextUninstallExternalWebAppResult(kWebAppUrl, false);
  base::RunLoop run_loop;
  task->Install(
      web_contents(), web_app::WebAppUrlLoader::Result::kUrlLoaded,
      base::BindLambdaForTesting(
          [&](BookmarkAppInstallationTask::Result result) {
            EXPECT_EQ(web_app::InstallResultCode::kFailedUnknownReason,
                      result.code);
            EXPECT_FALSE(result.app_id.has_value());
            EXPECT_TRUE(IsPlaceholderApp(profile(), kWebAppUrl));

            EXPECT_EQ(
                1u,
                install_finalizer()->uninstall_external_web_app_urls().size());
            EXPECT_EQ(
                kWebAppUrl,
                install_finalizer()->uninstall_external_web_app_urls().at(0));

            // There should have been no new calls to install a placeholder.
            EXPECT_EQ(1u, install_finalizer()->finalize_options_list().size());

            run_loop.Quit();
          }));
  run_loop.Run();
}

}  // namespace extensions
