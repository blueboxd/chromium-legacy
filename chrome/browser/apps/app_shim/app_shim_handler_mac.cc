// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/app_shim_handler_mac.h"

#include <map>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/apps/platform_apps/app_window_registry_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/mac/app_mode_common.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"

namespace apps {

namespace {

void TerminateIfNoAppWindows() {
  bool app_windows_left =
      AppWindowRegistryUtil::IsAppWindowVisibleInAnyProfile(0);
  if (!app_windows_left) {
    chrome::AttemptExit();
  }
}

class AppShimHandlerRegistry : public content::NotificationObserver {
 public:
  static AppShimHandlerRegistry* GetInstance() {
    return base::Singleton<
        AppShimHandlerRegistry,
        base::LeakySingletonTraits<AppShimHandlerRegistry>>::get();
  }

  AppShimHandler* GetHandler() const { return default_handler_; }

  void SetHandler(AppShimHandler* handler) { default_handler_ = handler; }

  void MaybeTerminate() {
    if (!browser_session_running_) {
      // Post this to give AppWindows a chance to remove themselves from the
      // registry.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&TerminateIfNoAppWindows));
    }
  }

  bool ShouldRestoreSession() {
    return !browser_session_running_;
  }

 private:
  friend struct base::DefaultSingletonTraits<AppShimHandlerRegistry>;
  typedef std::map<std::string, AppShimHandler*> HandlerMap;

  AppShimHandlerRegistry() {
    registrar_.Add(
        this, chrome::NOTIFICATION_BROWSER_OPENED,
        content::NotificationService::AllBrowserContextsAndSources());
    registrar_.Add(
        this, chrome::NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST,
        content::NotificationService::AllBrowserContextsAndSources());
    registrar_.Add(
        this, chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED,
        content::NotificationService::AllBrowserContextsAndSources());
  }

  ~AppShimHandlerRegistry() override {}

  // content::NotificationObserver override:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    switch (type) {
      case chrome::NOTIFICATION_BROWSER_OPENED:
      case chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED:
        browser_session_running_ = true;
        break;
      case chrome::NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST:
        browser_session_running_ = false;
        break;
      default:
        NOTREACHED();
    }
  }

  AppShimHandler* default_handler_ = nullptr;
  content::NotificationRegistrar registrar_;
  bool browser_session_running_ = false;

  DISALLOW_COPY_AND_ASSIGN(AppShimHandlerRegistry);
};

}  // namespace

// static
AppShimHandler* AppShimHandler::Get() {
  return AppShimHandlerRegistry::GetInstance()->GetHandler();
}

// static
void AppShimHandler::Set(AppShimHandler* handler) {
  AppShimHandlerRegistry::GetInstance()->SetHandler(handler);
}

// static
void AppShimHandler::MaybeTerminate() {
  AppShimHandlerRegistry::GetInstance()->MaybeTerminate();
}

// static
bool AppShimHandler::ShouldRestoreSession() {
  return AppShimHandlerRegistry::GetInstance()->ShouldRestoreSession();
}

}  // namespace apps
