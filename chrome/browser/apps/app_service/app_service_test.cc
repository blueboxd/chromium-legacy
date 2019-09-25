// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_service_test.h"

#include "base/run_loop.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"

namespace apps {

AppServiceTest::AppServiceTest() = default;

AppServiceTest::~AppServiceTest() = default;

void AppServiceTest::SetUp(Profile* profile) {
  app_service_proxy_ = apps::AppServiceProxyFactory::GetForProfile(profile);
  DCHECK(app_service_proxy_);
  app_service_proxy_->ReInitializeForTesting(profile);

  // Allow async callbacks to run.
  WaitForAppService();
}

std::string AppServiceTest::GetAppName(const std::string& app_id) const {
  std::string name;
  if (!app_service_proxy_)
    return name;
  app_service_proxy_->AppRegistryCache().ForOneApp(
      app_id, [&name](const apps::AppUpdate& update) { name = update.Name(); });
  return name;
}

void AppServiceTest::WaitForAppService() {
  base::RunLoop().RunUntilIdle();
}

void AppServiceTest::FlushMojoCalls() {
  if (app_service_proxy_) {
    app_service_proxy_->FlushMojoCallsForTesting();
  }
}

}  // namespace apps
