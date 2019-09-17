// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_WEB_APP_SYNC_BRIDGE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_WEB_APP_SYNC_BRIDGE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/web_applications/abstract_web_app_sync_bridge.h"

namespace web_app {

class TestWebAppSyncBridge : public AbstractWebAppSyncBridge {
 public:
  TestWebAppSyncBridge();
  ~TestWebAppSyncBridge() override;

  using AppIds = std::vector<AppId>;

  // AbstractWebAppSyncBridge:
  void OpenDatabase(RegistryOpenedCallback callback) override;
  void WriteWebApps(AppsToWrite apps, CompletionCallback callback) override;
  void DeleteWebApps(AppIds app_ids, CompletionCallback callback) override;

  RegistryOpenedCallback TakeOpenDatabaseCallback() {
    return std::move(open_database_callback_);
  }

  void SetNextWriteWebAppsResult(bool next_write_web_apps_result);
  void SetNextDeleteWebAppsResult(bool next_delete_web_apps_result);

  const AppIds& write_web_app_ids() const { return write_web_app_ids_; }
  const AppIds& delete_web_app_ids() const { return delete_web_app_ids_; }

 private:
  RegistryOpenedCallback open_database_callback_;
  AppIds write_web_app_ids_;
  AppIds delete_web_app_ids_;

  bool next_write_web_apps_result_ = true;
  bool next_delete_web_apps_result_ = true;

  DISALLOW_COPY_AND_ASSIGN(TestWebAppSyncBridge);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_WEB_APP_SYNC_BRIDGE_H_
