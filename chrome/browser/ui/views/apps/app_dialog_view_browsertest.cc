// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/arc_apps.h"
#include "chrome/browser/apps/app_service/arc_apps_factory.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/apps/app_block_dialog_view.h"
#include "chrome/browser/ui/views/apps/app_pause_dialog_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/arc/arc_util.h"
#include "components/arc/mojom/app.mojom.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_app_instance.h"

class AppDialogViewBrowserTest : public DialogBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    arc::ArcSessionManager::SetUiEnabledForTesting(false);
  }

  void SetUpOnMainThread() override {
    profile_ = browser()->profile();

    arc::SetArcPlayStoreEnabledForProfile(profile_, true);

    // Validating decoded content does not fit well for unit tests.
    ArcAppIcon::DisableSafeDecodingForTesting();

    arc_app_list_pref_ = ArcAppListPrefs::Get(profile_);
    DCHECK(arc_app_list_pref_);
    base::RunLoop run_loop;
    arc_app_list_pref_->SetDefaultAppsReadyCallback(run_loop.QuitClosure());
    run_loop.Run();

    app_instance_ = std::make_unique<arc::FakeAppInstance>(arc_app_list_pref_);
    arc_app_list_pref_->app_connection_holder()->SetInstance(
        app_instance_.get());
    WaitForInstanceReady(arc_app_list_pref_->app_connection_holder());
  }

  void TearDownOnMainThread() override {
    arc_app_list_pref_->app_connection_holder()->CloseInstance(
        app_instance_.get());
    app_instance_.reset();
    arc::ArcSessionManager::Get()->Shutdown();
  }

  AppDialogView* ActiveView(const std::string& name) {
    if (name == "block")
      return AppBlockDialogView::GetActiveViewForTesting();

    return AppPauseDialogView::GetActiveViewForTesting();
  }

  void ShowUi(const std::string& name) override {
    arc::mojom::AppInfo app;
    app.name = "Fake App 0";
    app.package_name = "fake.package.0";
    app.activity = "fake.app.0.activity";
    app.sticky = false;
    app_instance_->SendRefreshAppList(std::vector<arc::mojom::AppInfo>(1, app));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(1u, arc_app_list_pref_->GetAppIds().size());
    EXPECT_EQ(nullptr, ActiveView(name));

    auto* app_service_proxy =
        apps::AppServiceProxyFactory::GetForProfile(profile_);
    ASSERT_TRUE(app_service_proxy);

    if (name == "block") {
      app.suspended = true;
      app_instance_->SendRefreshAppList(
          std::vector<arc::mojom::AppInfo>(1, app));
    } else {
      std::string app_id =
          arc_app_list_pref_->GetAppId(app.package_name, app.activity);
      std::map<std::string, apps::PauseData> pause_data;
      pause_data[app_id].hours = 3;
      pause_data[app_id].minutes = 30;
      pause_data[app_id].should_show_pause_dialog = true;
      app_service_proxy->PauseApps(pause_data);
    }
    base::RunLoop run_loop;
    if (name == "block") {
      apps::ArcAppsFactory::GetForProfile(profile_)
          ->SetDialogCreatedCallbackForTesting(run_loop.QuitClosure());
    } else {
      app_service_proxy->SetDialogCreatedCallbackForTesting(
          run_loop.QuitClosure());
    }
    run_loop.Run();

    ASSERT_NE(nullptr, ActiveView(name));
    EXPECT_EQ(ui::DIALOG_BUTTON_OK, ActiveView(name)->GetDialogButtons());

    app_service_proxy->FlushMojoCallsForTesting();
    std::string app_id =
        arc_app_list_pref_->GetAppId(app.package_name, app.activity);
    bool state_is_set = false;
    app_service_proxy->AppRegistryCache().ForOneApp(
        app_id, [&state_is_set, name](const apps::AppUpdate& update) {
          if (name == "block") {
            state_is_set = (update.Readiness() ==
                            apps::mojom::Readiness::kDisabledByPolicy);
          } else {
            state_is_set =
                (update.Paused() == apps::mojom::OptionalBool::kTrue);
          }
        });

    EXPECT_TRUE(state_is_set);
  }

 private:
  Profile* profile_ = nullptr;
  ArcAppListPrefs* arc_app_list_pref_ = nullptr;
  std::unique_ptr<arc::FakeAppInstance> app_instance_;
};

IN_PROC_BROWSER_TEST_F(AppDialogViewBrowserTest, InvokeUi_block) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AppDialogViewBrowserTest, InvokeUi_pause) {
  ShowAndVerifyUi();
}
