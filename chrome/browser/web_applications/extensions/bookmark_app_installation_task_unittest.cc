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
#include "chrome/browser/web_applications/bookmark_apps/test_web_app_provider.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_data_retriever.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/test/test_app_registrar.h"
#include "chrome/browser/web_applications/test/test_data_retriever.h"
#include "chrome/browser/web_applications/test/test_install_finalizer.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "url/gurl.h"

namespace extensions {

using Result = BookmarkAppInstallationTask::Result;

namespace {

// Returns a factory that will return |data_retriever| the first time it gets
// called. It will DCHECK if called more than once.
web_app::WebAppInstallManager::DataRetrieverFactory GetFactoryForRetriever(
    std::unique_ptr<web_app::WebAppDataRetriever> data_retriever) {
  // Ideally we would return this lambda directly but passing a mutable lambda
  // to BindLambdaForTesting results in a OnceCallback which cannot be used as a
  // DataRetrieverFactory because DataRetrieverFactory is a RepeatingCallback.
  // For this reason, wrap the OnceCallback in a repeating callback that DCHECKs
  // if it gets called more than once.
  auto callback = base::BindLambdaForTesting(
      [data_retriever = std::move(data_retriever)]() mutable {
        return std::move(data_retriever);
      });

  return base::BindRepeating(
      [](base::OnceCallback<std::unique_ptr<web_app::WebAppDataRetriever>()>
             callback) {
        DCHECK(callback);
        return std::move(callback).Run();
      },
      base::Passed(std::move(callback)));
}

const GURL kWebAppUrl("https://foo.example");

// TODO(ortuno): Move this to ExternallyInstalledWebAppPrefs or replace with a
// method in ExternallyInstalledWebAppPrefs once there is one.
bool IsPlaceholderApp(Profile* profile, const GURL& url) {
  const base::Value* map =
      profile->GetPrefs()->GetDictionary(prefs::kWebAppsExtensionIDs);

  const base::Value* entry = map->FindKey(url.spec());

  return entry->FindBoolKey("is_placeholder").value();
}

class TestBookmarkAppInstallFinalizer : public web_app::InstallFinalizer {
 public:
  explicit TestBookmarkAppInstallFinalizer(web_app::TestAppRegistrar* registrar)
      : registrar_(registrar) {}
  ~TestBookmarkAppInstallFinalizer() override = default;

  // Returns what would be the AppId if an app is installed with |url|.
  web_app::AppId GetAppIdForUrl(const GURL& url) {
    return web_app::TestInstallFinalizer::GetAppIdForUrl(url);
  }

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
  size_t num_pin_app_to_shelf_calls() { return num_pin_app_to_shelf_calls_; }
  size_t num_reparent_tab_calls() { return num_reparent_tab_calls_; }
  size_t num_reveal_appshim_calls() { return num_reveal_appshim_calls_; }

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
              app_id, {url, web_app::ExternalInstallSource::kExternalPolicy});
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
    add_to_desktop_ = add_to_desktop;
    bool shortcut_created = next_create_os_shortcuts_results_[app_id];
    next_create_os_shortcuts_results_.erase(app_id);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), shortcut_created));
  }

  const base::Optional<bool>& add_to_desktop() { return add_to_desktop_; }

  bool CanPinAppToShelf() const override { return true; }

  void PinAppToShelf(const web_app::AppId& app_id) override {
    ++num_pin_app_to_shelf_calls_;
  }

  bool CanReparentTab(const web_app::AppId& app_id,
                      bool shortcut_created) const override {
    return true;
  }

  void ReparentTab(const web_app::AppId& app_id,
                   content::WebContents* web_contents) override {
    ++num_reparent_tab_calls_;
  }

  bool CanRevealAppShim() const override {
    return true;
  }

  void RevealAppShim(const web_app::AppId& app_id) override {
    ++num_reveal_appshim_calls_;
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
  base::Optional<bool> add_to_desktop_;

  size_t num_pin_app_to_shelf_calls_ = 0;
  size_t num_reparent_tab_calls_ = 0;
  size_t num_reveal_appshim_calls_ = 0;

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

class BookmarkAppInstallationTaskTest : public ChromeRenderViewHostTestHarness {
 public:
  BookmarkAppInstallationTaskTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kDesktopPWAsUnifiedInstall}, {});
  }

  ~BookmarkAppInstallationTaskTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    auto* provider = web_app::TestWebAppProvider::Get(profile());

    auto registrar = std::make_unique<web_app::TestAppRegistrar>();
    registrar_ = registrar.get();

    auto install_finalizer =
        std::make_unique<TestBookmarkAppInstallFinalizer>(registrar.get());
    install_finalizer_ = install_finalizer.get();

    auto data_retriever = std::make_unique<web_app::TestDataRetriever>();
    data_retriever_ = data_retriever.get();

    auto install_manager =
        std::make_unique<web_app::WebAppInstallManager>(profile());
    install_manager->SetDataRetrieverFactoryForTesting(
        GetFactoryForRetriever(std::move(data_retriever)));

    provider->SetRegistrar(std::move(registrar));
    provider->SetInstallManager(std::move(install_manager));
    provider->SetInstallFinalizer(std::move(install_finalizer));

    provider->Start();
  }

 protected:
  web_app::TestAppRegistrar* registrar() { return registrar_; }
  TestBookmarkAppInstallFinalizer* finalizer() { return install_finalizer_; }

  web_app::TestDataRetriever* data_retriever() { return data_retriever_; }

  const web_app::InstallFinalizer::FinalizeOptions& finalize_options() {
    DCHECK_EQ(1u, install_finalizer_->finalize_options_list().size());
    return install_finalizer_->finalize_options_list().at(0);
  }

  std::unique_ptr<BookmarkAppInstallationTask> GetInstallationTaskWithTestMocks(
      web_app::ExternalInstallOptions options) {
    auto manifest = std::make_unique<blink::Manifest>();
    manifest->start_url = options.url;

    data_retriever_->SetRendererWebApplicationInfo(
        std::make_unique<WebApplicationInfo>());

    data_retriever_->SetManifest(std::move(manifest), /*is_installable=*/true);

    data_retriever_->SetIcons(web_app::IconsMap{});

    install_finalizer_->SetNextFinalizeInstallResult(
        options.url, web_app::InstallResultCode::kSuccess);

    install_finalizer_->SetNextCreateOsShortcutsResult(
        install_finalizer_->GetAppIdForUrl(options.url), true);

    auto task = std::make_unique<BookmarkAppInstallationTask>(
        profile(), registrar_, install_finalizer_, std::move(options));
    return task;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  web_app::TestAppRegistrar* registrar_ = nullptr;
  web_app::TestDataRetriever* data_retriever_ = nullptr;
  TestBookmarkAppInstallFinalizer* install_finalizer_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(BookmarkAppInstallationTaskTest);
};

TEST_F(BookmarkAppInstallationTaskTest,
       WebAppOrShortcutFromContents_InstallationSucceeds) {
  auto task = GetInstallationTaskWithTestMocks(
      {kWebAppUrl, web_app::LaunchContainer::kDefault,
       web_app::ExternalInstallSource::kInternalDefault});

  base::RunLoop run_loop;

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

            EXPECT_EQ(1u, finalizer()->num_create_os_shortcuts_calls());
            EXPECT_TRUE(finalizer()->add_to_desktop().value());
            EXPECT_EQ(1u, finalizer()->num_pin_app_to_shelf_calls());
            EXPECT_EQ(0u, finalizer()->num_reparent_tab_calls());
            EXPECT_EQ(0u, finalizer()->num_reveal_appshim_calls());

            EXPECT_EQ(web_app::LaunchContainer::kDefault,
                      finalize_options().force_launch_container);
            EXPECT_EQ(WebappInstallSource::INTERNAL_DEFAULT,
                      finalize_options().install_source);

            run_loop.Quit();
          }));

  run_loop.Run();
}

TEST_F(BookmarkAppInstallationTaskTest,
       WebAppOrShortcutFromContents_InstallationFails) {
  auto task = GetInstallationTaskWithTestMocks(
      {kWebAppUrl, web_app::LaunchContainer::kWindow,
       web_app::ExternalInstallSource::kInternalDefault});
  data_retriever()->SetRendererWebApplicationInfo(nullptr);

  base::RunLoop run_loop;

  task->Install(
      web_contents(), web_app::WebAppUrlLoader::Result::kUrlLoaded,
      base::BindLambdaForTesting(
          [&](BookmarkAppInstallationTask::Result result) {
            base::Optional<web_app::AppId> id =
                web_app::ExternallyInstalledWebAppPrefs(profile()->GetPrefs())
                    .LookupAppId(kWebAppUrl);

            EXPECT_EQ(web_app::InstallResultCode::kGetWebApplicationInfoFailed,
                      result.code);
            EXPECT_FALSE(result.app_id.has_value());

            EXPECT_FALSE(id.has_value());

            run_loop.Quit();
          }));

  run_loop.Run();
}

TEST_F(BookmarkAppInstallationTaskTest,
       WebAppOrShortcutFromContents_NoDesktopShortcut) {
  web_app::ExternalInstallOptions install_options(
      kWebAppUrl, web_app::LaunchContainer::kWindow,
      web_app::ExternalInstallSource::kInternalDefault);
  install_options.add_to_desktop = false;
  auto task = GetInstallationTaskWithTestMocks(std::move(install_options));

  base::RunLoop run_loop;

  task->Install(web_contents(), web_app::WebAppUrlLoader::Result::kUrlLoaded,
                base::BindLambdaForTesting(
                    [&](BookmarkAppInstallationTask::Result result) {
                      EXPECT_EQ(web_app::InstallResultCode::kSuccess,
                                result.code);
                      EXPECT_TRUE(result.app_id.has_value());

                      EXPECT_EQ(1u,
                                finalizer()->num_create_os_shortcuts_calls());
                      EXPECT_FALSE(finalizer()->add_to_desktop().value());

                      EXPECT_EQ(1u, finalizer()->num_pin_app_to_shelf_calls());
                      EXPECT_EQ(0u, finalizer()->num_reparent_tab_calls());
                      EXPECT_EQ(0u, finalizer()->num_reveal_appshim_calls());

                      run_loop.Quit();
                    }));

  run_loop.Run();
}

TEST_F(BookmarkAppInstallationTaskTest,
       WebAppOrShortcutFromContents_NoQuickLaunchBarShortcut) {
  web_app::ExternalInstallOptions install_options(
      kWebAppUrl, web_app::LaunchContainer::kWindow,
      web_app::ExternalInstallSource::kInternalDefault);
  install_options.add_to_quick_launch_bar = false;
  auto task = GetInstallationTaskWithTestMocks(std::move(install_options));

  base::RunLoop run_loop;
  task->Install(web_contents(), web_app::WebAppUrlLoader::Result::kUrlLoaded,
                base::BindLambdaForTesting(
                    [&](BookmarkAppInstallationTask::Result result) {
                      EXPECT_EQ(web_app::InstallResultCode::kSuccess,
                                result.code);
                      EXPECT_TRUE(result.app_id.has_value());

                      EXPECT_EQ(1u,
                                finalizer()->num_create_os_shortcuts_calls());
                      EXPECT_TRUE(finalizer()->add_to_desktop().value());

                      EXPECT_EQ(0u, finalizer()->num_pin_app_to_shelf_calls());
                      EXPECT_EQ(0u, finalizer()->num_reparent_tab_calls());
                      EXPECT_EQ(0u, finalizer()->num_reveal_appshim_calls());

                      run_loop.Quit();
                    }));

  run_loop.Run();
}

TEST_F(
    BookmarkAppInstallationTaskTest,
    WebAppOrShortcutFromContents_NoDesktopShortcutAndNoQuickLaunchBarShortcut) {
  web_app::ExternalInstallOptions install_options(
      kWebAppUrl, web_app::LaunchContainer::kWindow,
      web_app::ExternalInstallSource::kInternalDefault);
  install_options.add_to_desktop = false;
  install_options.add_to_quick_launch_bar = false;
  auto task = GetInstallationTaskWithTestMocks(std::move(install_options));

  base::RunLoop run_loop;
  task->Install(web_contents(), web_app::WebAppUrlLoader::Result::kUrlLoaded,
                base::BindLambdaForTesting(
                    [&](BookmarkAppInstallationTask::Result result) {
                      EXPECT_EQ(web_app::InstallResultCode::kSuccess,
                                result.code);
                      EXPECT_TRUE(result.app_id.has_value());

                      EXPECT_EQ(1u,
                                finalizer()->num_create_os_shortcuts_calls());
                      EXPECT_FALSE(finalizer()->add_to_desktop().value());

                      EXPECT_EQ(0u, finalizer()->num_pin_app_to_shelf_calls());
                      EXPECT_EQ(0u, finalizer()->num_reparent_tab_calls());
                      EXPECT_EQ(0u, finalizer()->num_reveal_appshim_calls());

                      run_loop.Quit();
                    }));

  run_loop.Run();
}

TEST_F(BookmarkAppInstallationTaskTest,
       WebAppOrShortcutFromContents_ForcedContainerWindow) {
  auto install_options = web_app::ExternalInstallOptions(
      kWebAppUrl, web_app::LaunchContainer::kWindow,
      web_app::ExternalInstallSource::kInternalDefault);
  auto task = GetInstallationTaskWithTestMocks(std::move(install_options));

  base::RunLoop run_loop;
  task->Install(web_contents(), web_app::WebAppUrlLoader::Result::kUrlLoaded,
                base::BindLambdaForTesting(
                    [&](BookmarkAppInstallationTask::Result result) {
                      EXPECT_EQ(web_app::InstallResultCode::kSuccess,
                                result.code);
                      EXPECT_TRUE(result.app_id.has_value());

                      EXPECT_EQ(web_app::LaunchContainer::kWindow,
                                finalize_options().force_launch_container);

                      run_loop.Quit();
                    }));

  run_loop.Run();
}

TEST_F(BookmarkAppInstallationTaskTest,
       WebAppOrShortcutFromContents_ForcedContainerTab) {
  auto install_options = web_app::ExternalInstallOptions(
      kWebAppUrl, web_app::LaunchContainer::kTab,
      web_app::ExternalInstallSource::kInternalDefault);
  auto task = GetInstallationTaskWithTestMocks(std::move(install_options));

  base::RunLoop run_loop;
  task->Install(web_contents(), web_app::WebAppUrlLoader::Result::kUrlLoaded,
                base::BindLambdaForTesting(
                    [&](BookmarkAppInstallationTask::Result result) {
                      EXPECT_EQ(web_app::InstallResultCode::kSuccess,
                                result.code);
                      EXPECT_TRUE(result.app_id.has_value());

                      EXPECT_EQ(web_app::LaunchContainer::kTab,
                                finalize_options().force_launch_container);
                      run_loop.Quit();
                    }));

  run_loop.Run();
}

TEST_F(BookmarkAppInstallationTaskTest,
       WebAppOrShortcutFromContents_DefaultApp) {
  auto install_options = web_app::ExternalInstallOptions(
      kWebAppUrl, web_app::LaunchContainer::kDefault,
      web_app::ExternalInstallSource::kInternalDefault);
  auto task = GetInstallationTaskWithTestMocks(std::move(install_options));

  base::RunLoop run_loop;
  task->Install(web_contents(), web_app::WebAppUrlLoader::Result::kUrlLoaded,
                base::BindLambdaForTesting(
                    [&](BookmarkAppInstallationTask::Result result) {
                      EXPECT_EQ(web_app::InstallResultCode::kSuccess,
                                result.code);
                      EXPECT_TRUE(result.app_id.has_value());

                      EXPECT_EQ(WebappInstallSource::INTERNAL_DEFAULT,
                                finalize_options().install_source);
                      run_loop.Quit();
                    }));

  run_loop.Run();
}

TEST_F(BookmarkAppInstallationTaskTest,
       WebAppOrShortcutFromContents_AppFromPolicy) {
  auto install_options = web_app::ExternalInstallOptions(
      kWebAppUrl, web_app::LaunchContainer::kDefault,
      web_app::ExternalInstallSource::kExternalPolicy);
  auto task = GetInstallationTaskWithTestMocks(std::move(install_options));

  base::RunLoop run_loop;
  task->Install(web_contents(), web_app::WebAppUrlLoader::Result::kUrlLoaded,
                base::BindLambdaForTesting(
                    [&](BookmarkAppInstallationTask::Result result) {
                      EXPECT_EQ(web_app::InstallResultCode::kSuccess,
                                result.code);
                      EXPECT_TRUE(result.app_id.has_value());

                      EXPECT_EQ(WebappInstallSource::EXTERNAL_POLICY,
                                finalize_options().install_source);
                      run_loop.Quit();
                    }));

  run_loop.Run();
}

TEST_F(BookmarkAppInstallationTaskTest, InstallPlaceholder) {
  web_app::ExternalInstallOptions options(
      kWebAppUrl, web_app::LaunchContainer::kWindow,
      web_app::ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  auto task = GetInstallationTaskWithTestMocks(std::move(options));

  base::RunLoop run_loop;
  task->Install(
      web_contents(), web_app::WebAppUrlLoader::Result::kRedirectedUrlLoaded,
      base::BindLambdaForTesting([&](BookmarkAppInstallationTask::Result
                                         result) {
        EXPECT_EQ(web_app::InstallResultCode::kSuccess, result.code);
        EXPECT_TRUE(result.app_id.has_value());

        EXPECT_TRUE(IsPlaceholderApp(profile(), kWebAppUrl));

        EXPECT_EQ(1u, finalizer()->num_create_os_shortcuts_calls());
        EXPECT_EQ(1u, finalizer()->finalize_options_list().size());
        EXPECT_EQ(WebappInstallSource::EXTERNAL_POLICY,
                  finalize_options().install_source);
        const WebApplicationInfo& web_app_info =
            finalizer()->web_app_info_list().at(0);

        EXPECT_EQ(base::UTF8ToUTF16(kWebAppUrl.spec()), web_app_info.title);
        EXPECT_EQ(kWebAppUrl, web_app_info.app_url);
        EXPECT_TRUE(web_app_info.open_as_window);
        EXPECT_TRUE(web_app_info.icons.empty());

        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(BookmarkAppInstallationTaskTest, InstallPlaceholderTwice) {
  web_app::ExternalInstallOptions options(
      kWebAppUrl, web_app::LaunchContainer::kWindow,
      web_app::ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  web_app::AppId placeholder_app_id;

  // Install a placeholder app.
  {
    auto task = GetInstallationTaskWithTestMocks(options);
    base::RunLoop run_loop;
    task->Install(
        web_contents(), web_app::WebAppUrlLoader::Result::kRedirectedUrlLoaded,
        base::BindLambdaForTesting(
            [&](BookmarkAppInstallationTask::Result result) {
              EXPECT_EQ(web_app::InstallResultCode::kSuccess, result.code);
              placeholder_app_id = result.app_id.value();

              EXPECT_EQ(1u, finalizer()->finalize_options_list().size());
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  // Try to install it again.
  auto task = GetInstallationTaskWithTestMocks(options);
  base::RunLoop run_loop;
  task->Install(
      web_contents(), web_app::WebAppUrlLoader::Result::kRedirectedUrlLoaded,
      base::BindLambdaForTesting(
          [&](BookmarkAppInstallationTask::Result result) {
            EXPECT_EQ(web_app::InstallResultCode::kSuccess, result.code);
            EXPECT_EQ(placeholder_app_id, result.app_id.value());

            // There shouldn't be a second call to the finalizer.
            EXPECT_EQ(1u, finalizer()->finalize_options_list().size());

            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(BookmarkAppInstallationTaskTest, ReinstallPlaceholderSucceeds) {
  web_app::ExternalInstallOptions options(
      kWebAppUrl, web_app::LaunchContainer::kWindow,
      web_app::ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  web_app::AppId placeholder_app_id;

  // Install a placeholder app.
  {
    auto task = GetInstallationTaskWithTestMocks(options);

    base::RunLoop run_loop;
    task->Install(
        web_contents(), web_app::WebAppUrlLoader::Result::kRedirectedUrlLoaded,
        base::BindLambdaForTesting(
            [&](BookmarkAppInstallationTask::Result result) {
              EXPECT_EQ(web_app::InstallResultCode::kSuccess, result.code);
              placeholder_app_id = result.app_id.value();

              EXPECT_EQ(1u, finalizer()->finalize_options_list().size());
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  // Replace the placeholder with a real app.
  options.reinstall_placeholder = true;
  auto task = GetInstallationTaskWithTestMocks(options);
  finalizer()->SetNextUninstallExternalWebAppResult(kWebAppUrl, true);

  base::RunLoop run_loop;
  task->Install(
      web_contents(), web_app::WebAppUrlLoader::Result::kUrlLoaded,
      base::BindLambdaForTesting(
          [&](BookmarkAppInstallationTask::Result result) {
            EXPECT_EQ(web_app::InstallResultCode::kSuccess, result.code);
            EXPECT_TRUE(result.app_id.has_value());
            EXPECT_FALSE(IsPlaceholderApp(profile(), kWebAppUrl));

            EXPECT_EQ(1u,
                      finalizer()->uninstall_external_web_app_urls().size());
            EXPECT_EQ(kWebAppUrl,
                      finalizer()->uninstall_external_web_app_urls().at(0));

            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(BookmarkAppInstallationTaskTest, ReinstallPlaceholderFails) {
  web_app::ExternalInstallOptions options(
      kWebAppUrl, web_app::LaunchContainer::kWindow,
      web_app::ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  web_app::AppId placeholder_app_id;

  // Install a placeholder app.
  {
    auto task = GetInstallationTaskWithTestMocks(options);
    base::RunLoop run_loop;
    task->Install(
        web_contents(), web_app::WebAppUrlLoader::Result::kRedirectedUrlLoaded,
        base::BindLambdaForTesting(
            [&](BookmarkAppInstallationTask::Result result) {
              EXPECT_EQ(web_app::InstallResultCode::kSuccess, result.code);
              placeholder_app_id = result.app_id.value();

              EXPECT_EQ(1u, finalizer()->finalize_options_list().size());

              run_loop.Quit();
            }));
    run_loop.Run();
  }

  // Replace the placeholder with a real app.
  options.reinstall_placeholder = true;
  auto task = GetInstallationTaskWithTestMocks(options);

  finalizer()->SetNextUninstallExternalWebAppResult(kWebAppUrl, false);

  base::RunLoop run_loop;
  task->Install(
      web_contents(), web_app::WebAppUrlLoader::Result::kUrlLoaded,
      base::BindLambdaForTesting(
          [&](BookmarkAppInstallationTask::Result result) {
            EXPECT_EQ(web_app::InstallResultCode::kFailedUnknownReason,
                      result.code);
            EXPECT_FALSE(result.app_id.has_value());
            EXPECT_TRUE(IsPlaceholderApp(profile(), kWebAppUrl));

            EXPECT_EQ(1u,
                      finalizer()->uninstall_external_web_app_urls().size());
            EXPECT_EQ(kWebAppUrl,
                      finalizer()->uninstall_external_web_app_urls().at(0));

            // There should have been no new calls to install a placeholder.
            EXPECT_EQ(1u, finalizer()->finalize_options_list().size());

            run_loop.Quit();
          }));
  run_loop.Run();
}

}  // namespace extensions
