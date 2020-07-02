// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/system_web_app_manager.h"

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/web_applications/components/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/browser/web_applications/pending_app_manager_impl.h"
#include "chrome/browser/web_applications/test/test_app_shortcut_manager.h"
#include "chrome/browser/web_applications/test/test_data_retriever.h"
#include "chrome/browser/web_applications/test/test_file_handler_manager.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/test_pending_app_manager_impl.h"
#include "chrome/browser/web_applications/test/test_system_web_app_manager.h"
#include "chrome/browser/web_applications/test/test_web_app_database_factory.h"
#include "chrome/browser/web_applications/test/test_web_app_registry_controller.h"
#include "chrome/browser/web_applications/test/test_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "content/public/test/test_utils.h"
#include "url/gurl.h"

namespace web_app {

namespace {
const char kSettingsAppNameForLogging[] = "OSSettings";
const char kDiscoverAppNameForLogging[] = "Discover";

GURL AppUrl1() {
  return GURL(content::GetWebUIURL("system-app1"));
}
GURL AppIconUrl1() {
  return GURL(content::GetWebUIURL("system-app1/app.ico"));
}
GURL AppUrl2() {
  return GURL(content::GetWebUIURL("system-app2"));
}
GURL AppIconUrl2() {
  return GURL(content::GetWebUIURL("system-app2/app.ico"));
}

struct SystemAppData {
  GURL url;
  GURL icon_url;
  ExternalInstallSource source;
};

class TestDataRetrieverFactory {
 public:
  TestDataRetrieverFactory() = delete;
  explicit TestDataRetrieverFactory(std::vector<SystemAppData> system_app_data)
      : system_app_data_(std::move(system_app_data)) {}

  std::unique_ptr<web_app::WebAppDataRetriever> CreateNextDataRetriever() {
    size_t task_index = task_index_++;

    auto data_retriever = std::make_unique<TestDataRetriever>();
    data_retriever->SetEmptyRendererWebApplicationInfo();

    // System apps require an icon specified in the manifest.
    auto manifest = std::make_unique<blink::Manifest>();
    manifest->start_url = GetSystemAppDataForTask(task_index).url;
    manifest->scope = GetSystemAppDataForTask(task_index).url;
    blink::Manifest::ImageResource icon;
    icon.src = GetSystemAppDataForTask(task_index).icon_url;
    icon.purpose.push_back(blink::Manifest::ImageResource::Purpose::ANY);
    icon.sizes.emplace_back(gfx::Size(icon_size::k256, icon_size::k256));
    manifest->icons.push_back(std::move(icon));
    data_retriever->SetManifest(std::move(manifest),
                                /*is_installable=*/true);

    TestDataRetriever* data_retriever_ptr = data_retriever.get();
    task_data_retrievers_.insert(data_retriever_ptr);

    // Every InstallTask starts with WebAppDataRetriever::GetIcons step.
    data_retriever->SetGetIconsDelegate(base::BindLambdaForTesting(
        [&, task_index](content::WebContents* web_contents,
                        const std::vector<GURL>& icon_urls,
                        bool skip_page_favicons) {
          IconsMap icons_map;
          AddIconToIconsMap(GetSystemAppDataForTask(task_index).icon_url,
                            icon_size::k256, SK_ColorBLUE, &icons_map);
          return icons_map;
        }));

    // Every InstallTask ends with WebAppDataRetriever destructor.
    data_retriever->SetDestructionCallback(
        base::BindLambdaForTesting([this, data_retriever_ptr]() {
          task_data_retrievers_.erase(data_retriever_ptr);
        }));

    return std::unique_ptr<WebAppDataRetriever>(std::move(data_retriever));
  }

 private:
  const SystemAppData& GetSystemAppDataForTask(size_t task_index) const {
    DCHECK(task_index < system_app_data_.size());
    return system_app_data_[task_index];
  }

  size_t task_index_ = 0;
  std::vector<SystemAppData> system_app_data_;
  base::flat_set<TestDataRetriever*> task_data_retrievers_;
};

}  // namespace

class SystemWebAppManagerTest : public WebAppTest {
 public:
  SystemWebAppManagerTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kSystemWebApps, features::kDesktopPWAsWithoutExtensions},
        {});
  }

  ~SystemWebAppManagerTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();

    test_registry_controller_ =
        std::make_unique<TestWebAppRegistryController>();

    controller().SetUp(profile());

    externally_installed_app_prefs_ =
        std::make_unique<ExternallyInstalledWebAppPrefs>(profile()->GetPrefs());
    test_file_handler_manager_ =
        std::make_unique<TestFileHandlerManager>(profile());
    icon_manager_ = std::make_unique<WebAppIconManager>(
        profile(), controller().registrar(), std::make_unique<TestFileUtils>());
    install_finalizer_ = std::make_unique<WebAppInstallFinalizer>(
        profile(), &icon_manager(), /*legacy_finalizer=*/nullptr);
    install_manager_ = std::make_unique<WebAppInstallManager>(profile());
    test_pending_app_manager_impl_ =
        std::make_unique<TestPendingAppManagerImpl>(profile());
    test_shortcut_manager_ =
        std::make_unique<TestAppShortcutManager>(profile());
    test_system_web_app_manager_ =
        std::make_unique<TestSystemWebAppManager>(profile());
    test_ui_manager_ = std::make_unique<TestWebAppUiManager>();

    install_finalizer().SetSubsystems(&controller().registrar(), &ui_manager(),
                                      &controller().sync_bridge());

    install_manager().SetUrlLoaderForTesting(
        std::make_unique<TestWebAppUrlLoader>());
    install_manager().SetSubsystems(
        &controller().registrar(), &shortcut_manager(), &file_handler_manager(),
        &install_finalizer());

    auto url_loader = std::make_unique<TestWebAppUrlLoader>();
    url_loader_ = url_loader.get();
    pending_app_manager().SetUrlLoaderForTesting(std::move(url_loader));
    pending_app_manager().SetSubsystems(
        &controller().registrar(), &shortcut_manager(), &file_handler_manager(),
        &ui_manager(), &install_finalizer(), &install_manager());

    system_web_app_manager().SetSubsystems(
        &pending_app_manager(), &controller().registrar(),
        &controller().sync_bridge(), &ui_manager(), &file_handler_manager());

    install_manager().Start();
    install_finalizer().Start();
  }

  void TearDown() override {
    DestroyManagers();
    WebAppTest::TearDown();
  }

  void DestroyManagers() {
    // The reverse order of creation:
    test_ui_manager_.reset();
    test_system_web_app_manager_.reset();
    test_shortcut_manager_.reset();
    test_pending_app_manager_impl_.reset();
    install_manager_.reset();
    install_finalizer_.reset();
    icon_manager_.reset();
    test_file_handler_manager_.reset();
    externally_installed_app_prefs_.reset();
    test_registry_controller_.reset();
  }

 protected:
  TestWebAppRegistryController& controller() {
    return *test_registry_controller_;
  }

  ExternallyInstalledWebAppPrefs& externally_installed_app_prefs() {
    return *externally_installed_app_prefs_;
  }

  TestFileHandlerManager& file_handler_manager() {
    return *test_file_handler_manager_;
  }

  WebAppIconManager& icon_manager() { return *icon_manager_; }

  WebAppInstallFinalizer& install_finalizer() { return *install_finalizer_; }

  WebAppInstallManager& install_manager() { return *install_manager_; }

  TestPendingAppManagerImpl& pending_app_manager() {
    return *test_pending_app_manager_impl_;
  }

  TestAppShortcutManager& shortcut_manager() { return *test_shortcut_manager_; }

  TestSystemWebAppManager& system_web_app_manager() {
    return *test_system_web_app_manager_;
  }

  TestWebAppUiManager& ui_manager() { return *test_ui_manager_; }

  TestWebAppUrlLoader& url_loader() { return *url_loader_; }

  std::unique_ptr<WebApp> CreateWebApp(
      const GURL& launch_url,
      Source::Type source_type = Source::kDefault) {
    const AppId app_id = GenerateAppIdFromURL(launch_url);

    auto web_app = std::make_unique<WebApp>(app_id);
    web_app->SetLaunchUrl(launch_url);
    web_app->AddSource(source_type);
    web_app->SetDisplayMode(DisplayMode::kStandalone);
    web_app->SetUserDisplayMode(DisplayMode::kStandalone);
    return web_app;
  }

  std::unique_ptr<WebApp> CreateSystemWebApp(const GURL& launch_url) {
    return CreateWebApp(launch_url, Source::Type::kSystem);
  }

  void InitRegistrarWithRegistry(const Registry& registry) {
    controller().database_factory().WriteRegistry(registry);
    controller().Init();
  }

  void InitRegistrarWithSystemApps(
      std::vector<SystemAppData> system_app_data_list) {
    DCHECK(controller().registrar().is_empty());
    DCHECK(!system_app_data_list.empty());

    Registry registry;
    for (const SystemAppData& data : system_app_data_list) {
      std::unique_ptr<WebApp> web_app = CreateSystemWebApp(data.url);
      const AppId app_id = web_app->app_id();
      registry.emplace(app_id, std::move(web_app));

      externally_installed_app_prefs().Insert(
          data.url, GenerateAppIdFromURL(data.url), data.source);
    }
    InitRegistrarWithRegistry(registry);
  }

  void InitEmptyRegistrar() {
    Registry registry;
    InitRegistrarWithRegistry(registry);
  }

  void PrepareSystemAppDataToRetrieve(
      std::vector<SystemAppData> system_app_data) {
    DCHECK(!test_data_retriever_factory_);
    test_data_retriever_factory_ =
        std::make_unique<TestDataRetrieverFactory>(std::move(system_app_data));
    install_manager().SetDataRetrieverFactoryForTesting(
        base::BindLambdaForTesting([this]() {
          DCHECK(test_data_retriever_factory_);
          return test_data_retriever_factory_->CreateNextDataRetriever();
        }));
  }

  void WaitForAppsToSynchronize() {
    base::RunLoop run_loop;
    system_web_app_manager().on_apps_synchronized().Post(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          // Wait one execution loop for on_apps_synchronized() to be
          // called on all listeners.
          base::SequencedTaskRunnerHandle::Get()->PostTask(
              FROM_HERE, run_loop.QuitClosure());
        }));
    run_loop.Run();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestWebAppRegistryController> test_registry_controller_;
  std::unique_ptr<ExternallyInstalledWebAppPrefs>
      externally_installed_app_prefs_;
  std::unique_ptr<TestFileHandlerManager> test_file_handler_manager_;
  std::unique_ptr<WebAppIconManager> icon_manager_;
  std::unique_ptr<WebAppInstallFinalizer> install_finalizer_;
  std::unique_ptr<WebAppInstallManager> install_manager_;
  std::unique_ptr<TestPendingAppManagerImpl> test_pending_app_manager_impl_;
  std::unique_ptr<TestAppShortcutManager> test_shortcut_manager_;
  std::unique_ptr<TestSystemWebAppManager> test_system_web_app_manager_;
  std::unique_ptr<TestWebAppUiManager> test_ui_manager_;
  TestWebAppUrlLoader* url_loader_ = nullptr;
  std::unique_ptr<TestDataRetrieverFactory> test_data_retriever_factory_;

  DISALLOW_COPY_AND_ASSIGN(SystemWebAppManagerTest);
};

// Test that System Apps are uninstalled with the feature disabled.
TEST_F(SystemWebAppManagerTest, Disabled) {
  base::test::ScopedFeatureList disable_feature_list;
  disable_feature_list.InitAndDisableFeature(features::kSystemWebApps);

  InitRegistrarWithSystemApps(
      {{AppUrl1(), AppIconUrl1(), ExternalInstallSource::kSystemInstalled}});

  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps.emplace(SystemAppType::SETTINGS,
                      SystemAppInfo(kSettingsAppNameForLogging, AppUrl1()));

  system_web_app_manager().SetSystemAppsForTesting(std::move(system_apps));
  system_web_app_manager().Start();

  WaitForAppsToSynchronize();

  EXPECT_TRUE(pending_app_manager().install_requests().empty());

  // We should try to uninstall the app that is no longer in the System App
  // list.
  EXPECT_EQ(std::vector<GURL>({AppUrl1()}),
            pending_app_manager().uninstall_requests());
}

// Test that System Apps do install with the feature enabled.
TEST_F(SystemWebAppManagerTest, Enabled) {
  InitEmptyRegistrar();

  PrepareSystemAppDataToRetrieve(
      {{AppUrl1(), AppIconUrl1()}, {AppUrl2(), AppIconUrl2()}});

  url_loader().AddPrepareForLoadResults({WebAppUrlLoader::Result::kUrlLoaded,
                                         WebAppUrlLoader::Result::kUrlLoaded});
  url_loader().SetNextLoadUrlResult(AppUrl1(),
                                    WebAppUrlLoader::Result::kUrlLoaded);
  url_loader().SetNextLoadUrlResult(AppUrl2(),
                                    WebAppUrlLoader::Result::kUrlLoaded);

  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps.emplace(SystemAppType::SETTINGS,
                      SystemAppInfo(kSettingsAppNameForLogging, AppUrl1()));
  system_apps.emplace(SystemAppType::DISCOVER,
                      SystemAppInfo(kDiscoverAppNameForLogging, AppUrl2()));

  system_web_app_manager().SetSystemAppsForTesting(std::move(system_apps));
  system_web_app_manager().Start();

  WaitForAppsToSynchronize();

  EXPECT_FALSE(pending_app_manager().install_requests().empty());
}

}  // namespace web_app
