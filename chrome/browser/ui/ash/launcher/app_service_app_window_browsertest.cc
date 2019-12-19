// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "ash/public/cpp/shelf_model.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/services/app_service/public/cpp/instance.h"
#include "chrome/services/app_service/public/cpp/instance_registry.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"

using extensions::AppWindow;
using extensions::Extension;

namespace {

ash::ShelfAction SelectItem(
    const ash::ShelfID& id,
    ui::EventType event_type = ui::ET_MOUSE_PRESSED,
    int64_t display_id = display::kInvalidDisplayId,
    ash::ShelfLaunchSource source = ash::LAUNCH_FROM_UNKNOWN) {
  return SelectShelfItem(id, event_type, display_id, source);
}

}  // namespace

class AppServiceAppWindowBrowserTest
    : public extensions::PlatformAppBrowserTest {
 protected:
  AppServiceAppWindowBrowserTest() : controller_(nullptr) {}

  ~AppServiceAppWindowBrowserTest() override {}

  void SetUp() override {
    if (!base::FeatureList::IsEnabled(features::kAppServiceInstanceRegistry)) {
      GTEST_SKIP() << "skipping all tests because kAppServiceInstanceRegistry "
                      "is not enabled";
    }
    extensions::PlatformAppBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    controller_ = ChromeLauncherController::instance();
    ASSERT_TRUE(controller_);
    extensions::PlatformAppBrowserTest::SetUpOnMainThread();

    app_service_proxy_ = apps::AppServiceProxyFactory::GetForProfile(profile());
    ASSERT_TRUE(app_service_proxy_);
  }

  ash::ShelfModel* shelf_model() { return controller_->shelf_model(); }

  // Returns the last item in the shelf.
  const ash::ShelfItem& GetLastLauncherItem() {
    return shelf_model()->items()[shelf_model()->item_count() - 1];
  }

  ChromeLauncherController* controller_ = nullptr;
  apps::AppServiceProxy* app_service_proxy_ = nullptr;
};

// Test that we have the correct instance for Chrome apps.
IN_PROC_BROWSER_TEST_F(AppServiceAppWindowBrowserTest, ExtensionAppsWindow) {
  const extensions::Extension* app =
      LoadAndLaunchPlatformApp("launch", "Launched");
  extensions::AppWindow* window = CreateAppWindow(profile(), app);
  ASSERT_TRUE(window);

  auto windows = app_service_proxy_->InstanceRegistry().GetWindows(app->id());
  EXPECT_EQ(1u, windows.size());
  apps::InstanceState latest_state = apps::InstanceState::kUnknown;
  app_service_proxy_->InstanceRegistry().ForOneInstance(
      *windows.begin(),
      [&app, &latest_state](const apps::InstanceUpdate& inner) {
        if (inner.AppId() == app->id()) {
          latest_state = inner.State();
        }
      });

  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            latest_state);

  const ash::ShelfItem& item = GetLastLauncherItem();
  // Since it is already active, clicking it should minimize.
  SelectItem(item.id);
  latest_state = apps::InstanceState::kUnknown;
  app_service_proxy_->InstanceRegistry().ForOneInstance(
      *windows.begin(),
      [&app, &latest_state](const apps::InstanceUpdate& inner) {
        if (inner.AppId() == app->id()) {
          latest_state = inner.State();
        }
      });
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning,
            latest_state);

  // Click the item again to activate the app.
  SelectItem(item.id);
  latest_state = apps::InstanceState::kUnknown;
  app_service_proxy_->InstanceRegistry().ForOneInstance(
      *windows.begin(),
      [&app, &latest_state](const apps::InstanceUpdate& inner) {
        if (inner.AppId() == app->id()) {
          latest_state = inner.State();
        }
      });
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            latest_state);

  CloseAppWindow(window);
  windows = app_service_proxy_->InstanceRegistry().GetWindows(app->id());
  EXPECT_EQ(0u, windows.size());
}

// Test that we have the correct instances with more than one window.
IN_PROC_BROWSER_TEST_F(AppServiceAppWindowBrowserTest, MultipleWindows) {
  const extensions::Extension* app =
      LoadAndLaunchPlatformApp("launch", "Launched");
  extensions::AppWindow* app_window1 = CreateAppWindow(profile(), app);

  auto windows = app_service_proxy_->InstanceRegistry().GetWindows(app->id());
  auto* window1 = *windows.begin();

  // Add a second window; confirm the shelf item stays; check the app menu.
  extensions::AppWindow* app_window2 = CreateAppWindow(profile(), app);

  windows = app_service_proxy_->InstanceRegistry().GetWindows(app->id());
  EXPECT_EQ(2u, windows.size());
  aura::Window* window2 = nullptr;
  for (auto* window : windows) {
    if (window != window1)
      window2 = window;
  }

  // The window1 is inactive.
  apps::InstanceState latest_state = apps::InstanceState::kUnknown;
  app_service_proxy_->InstanceRegistry().ForOneInstance(
      window1, [&app, &latest_state](const apps::InstanceUpdate& inner) {
        if (inner.AppId() == app->id()) {
          latest_state = inner.State();
        }
      });
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kVisible,
            latest_state);

  // The window2 is active.
  latest_state = apps::InstanceState::kUnknown;
  app_service_proxy_->InstanceRegistry().ForOneInstance(
      window2, [&app, &latest_state](const apps::InstanceUpdate& inner) {
        if (inner.AppId() == app->id()) {
          latest_state = inner.State();
        }
      });
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            latest_state);

  // Close the second window; confirm the shelf item stays; check the app menu.
  CloseAppWindow(app_window2);
  windows = app_service_proxy_->InstanceRegistry().GetWindows(app->id());
  EXPECT_EQ(1u, windows.size());

  // The window1 is active again.
  latest_state = apps::InstanceState::kUnknown;
  app_service_proxy_->InstanceRegistry().ForOneInstance(
      window1, [&app, &latest_state](const apps::InstanceUpdate& inner) {
        if (inner.AppId() == app->id()) {
          latest_state = inner.State();
        }
      });
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            latest_state);

  // Close the first window; the shelf item should be removed.
  CloseAppWindow(app_window1);
  windows = app_service_proxy_->InstanceRegistry().GetWindows(app->id());
  EXPECT_EQ(0u, windows.size());
}

// Test that we have the correct instances with one HostedApp and one window.
IN_PROC_BROWSER_TEST_F(AppServiceAppWindowBrowserTest,
                       HostedAppandExtensionApp) {
  const extensions::Extension* extension1 = InstallHostedApp();
  LaunchHostedApp(extension1);

  std::string app_id1 = extension1->id();
  auto windows = app_service_proxy_->InstanceRegistry().GetWindows(app_id1);
  EXPECT_EQ(1u, windows.size());
  auto* window1 = *windows.begin();

  // The window1 is active.
  apps::InstanceState latest_state = apps::InstanceState::kUnknown;
  app_service_proxy_->InstanceRegistry().ForOneInstance(
      window1, [&app_id1, &latest_state](const apps::InstanceUpdate& inner) {
        if (inner.AppId() == app_id1) {
          latest_state = inner.State();
        }
      });
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kVisible | apps::InstanceState::kActive,
            latest_state);

  // Add an Extension app.
  const extensions::Extension* extension2 =
      LoadAndLaunchPlatformApp("launch", "Launched");
  auto* app_window = CreateAppWindow(profile(), extension2);

  std::string app_id2 = extension2->id();
  windows = app_service_proxy_->InstanceRegistry().GetWindows(app_id2);
  EXPECT_EQ(1u, windows.size());
  auto* window2 = *windows.begin();

  // The window1 is inactive.
  latest_state = apps::InstanceState::kUnknown;
  app_service_proxy_->InstanceRegistry().ForOneInstance(
      window1, [&app_id1, &latest_state](const apps::InstanceUpdate& inner) {
        if (inner.AppId() == app_id1) {
          latest_state = inner.State();
        }
      });
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kVisible,
            latest_state);

  // The window2 is active.
  latest_state = apps::InstanceState::kUnknown;
  app_service_proxy_->InstanceRegistry().ForOneInstance(
      window2, [&app_id2, &latest_state](const apps::InstanceUpdate& inner) {
        if (inner.AppId() == app_id2) {
          latest_state = inner.State();
        }
      });
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kVisible | apps::InstanceState::kActive,
            latest_state);

  // Close the Extension app's window..
  CloseAppWindow(app_window);
  windows = app_service_proxy_->InstanceRegistry().GetWindows(app_id2);
  EXPECT_EQ(0u, windows.size());

  // The window1 is active.
  latest_state = apps::InstanceState::kUnknown;
  app_service_proxy_->InstanceRegistry().ForOneInstance(
      window1, [&app_id1, &latest_state](const apps::InstanceUpdate& inner) {
        if (inner.AppId() == app_id1) {
          latest_state = inner.State();
        }
      });
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kVisible | apps::InstanceState::kActive,
            latest_state);

  // Close the HostedApp.
  TabStripModel* tab_strip = browser()->tab_strip_model();
  tab_strip->CloseWebContentsAt(tab_strip->active_index(),
                                TabStripModel::CLOSE_NONE);

  windows = app_service_proxy_->InstanceRegistry().GetWindows(app_id1);
  EXPECT_EQ(0u, windows.size());
}

class AppServiceAppWindowWebAppBrowserTest
    : public AppServiceAppWindowBrowserTest {
 protected:
  // AppServiceAppWindowBrowserTest:
  void SetUpOnMainThread() override {
    AppServiceAppWindowBrowserTest::SetUpOnMainThread();

    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());
  }

  // |SetUpWebApp()| must be called after |SetUpOnMainThread()| to make sure
  // the Network Service process has been setup properly.
  std::string CreateWebApp() const {
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->app_url = GetAppURL();
    web_app_info->scope = GetAppURL().GetWithoutFilename();

    std::string app_id =
        web_app::InstallWebApp(browser()->profile(), std::move(web_app_info));
    content::TestNavigationObserver navigation_observer(GetAppURL());
    navigation_observer.StartWatchingNewWebContents();
    // Browser* app_browser_ = nullptr;
    web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
    navigation_observer.WaitForNavigationFinished();

    return app_id;
  }

  GURL GetAppURL() const {
    return https_server_.GetURL("app.com", "/ssl/google.html");
  }

  // For mocking a secure site.
  net::EmbeddedTestServer https_server_;
};

// Test that we have the correct instance for Web apps.
IN_PROC_BROWSER_TEST_F(AppServiceAppWindowWebAppBrowserTest, WebAppsWindow) {
  std::string app_id = CreateWebApp();

  auto windows = app_service_proxy_->InstanceRegistry().GetWindows(app_id);
  EXPECT_EQ(1u, windows.size());
  aura::Window* window = *windows.begin();
  apps::InstanceState latest_state = apps::InstanceState::kUnknown;
  app_service_proxy_->InstanceRegistry().ForOneInstance(
      window, [&app_id, &latest_state](const apps::InstanceUpdate& inner) {
        if (inner.AppId() == app_id) {
          latest_state = inner.State();
        }
      });

  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            latest_state);

  const ash::ShelfItem& item = GetLastLauncherItem();
  // Since it is already active, clicking it should minimize.
  SelectItem(item.id);
  latest_state = apps::InstanceState::kUnknown;
  app_service_proxy_->InstanceRegistry().ForOneInstance(
      window, [&app_id, &latest_state](const apps::InstanceUpdate& inner) {
        if (inner.AppId() == app_id) {
          latest_state = inner.State();
        }
      });
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning,
            latest_state);

  // Click the item again to activate the app.
  SelectItem(item.id);
  latest_state = apps::InstanceState::kUnknown;
  app_service_proxy_->InstanceRegistry().ForOneInstance(
      window, [&app_id, &latest_state](const apps::InstanceUpdate& inner) {
        if (inner.AppId() == app_id) {
          latest_state = inner.State();
        }
      });
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            latest_state);

  controller_->Close(item.id);
  // Make sure that the window is closed.
  base::RunLoop().RunUntilIdle();
  windows = app_service_proxy_->InstanceRegistry().GetWindows(app_id);
  EXPECT_EQ(0u, windows.size());
}
