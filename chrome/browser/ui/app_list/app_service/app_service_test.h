// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_SERVICE_APP_SERVICE_TEST_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_SERVICE_APP_SERVICE_TEST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"

namespace service_manager {
class Connector;
class Service;
}  // namespace service_manager

namespace apps {
class AppServiceProxy;
}

class Profile;

// Helper class to initialize AppService in unit tests.
class AppServiceTest {
 public:
  AppServiceTest();
  ~AppServiceTest();

  void SetUp(Profile* profile);

  // Allow AppService async callbacks to run.
  void WaitForAppService();

  // Flush mojo calls to allow AppService async callbacks to run.
  void FlushMojoCallsForAppService();

 private:
  service_manager::Connector* app_service_proxy_connector_ = nullptr;
  apps::AppServiceProxy* app_service_proxy_ = nullptr;

  service_manager::TestConnectorFactory test_connector_factory_;
  std::unique_ptr<service_manager::Service> app_service_;

  DISALLOW_COPY_AND_ASSIGN(AppServiceTest);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_SERVICE_APP_SERVICE_TEST_H_
