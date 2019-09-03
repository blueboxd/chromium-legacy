// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_WEB_APP_DATABASE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_WEB_APP_DATABASE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/web_applications/abstract_web_app_database.h"

namespace web_app {

class TestWebAppDatabase : public AbstractWebAppDatabase {
 public:
  TestWebAppDatabase();
  ~TestWebAppDatabase() override;

  // AbstractWebAppDatabase:
  void OpenDatabase(OnceRegistryOpenedCallback callback) override;
  void WriteWebApps(AppsToWrite apps, CompletionCallback callback) override;
  void DeleteWebApps(std::vector<AppId> app_ids,
                     CompletionCallback callback) override;

  OnceRegistryOpenedCallback TakeOpenDatabaseCallback() {
    return std::move(open_database_callback_);
  }

  void SetNextWriteWebAppsResult(bool next_write_web_apps_result);
  void SetNextDeleteWebAppsResult(bool next_delete_web_apps_result);

  using AppIds = std::vector<AppId>;

  const AppIds& write_web_app_ids() const { return write_web_app_ids_; }
  const AppIds& delete_web_app_ids() const { return delete_web_app_ids_; }

 private:
  OnceRegistryOpenedCallback open_database_callback_;
  AppIds write_web_app_ids_;
  AppIds delete_web_app_ids_;

  bool next_write_web_apps_result_ = true;
  bool next_delete_web_apps_result_ = true;

  DISALLOW_COPY_AND_ASSIGN(TestWebAppDatabase);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_WEB_APP_DATABASE_H_
