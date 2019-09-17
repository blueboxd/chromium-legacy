// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_service/app_service_test.h"

#include "base/run_loop.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/services/app_service/app_service.h"
#include "chrome/services/app_service/public/mojom/constants.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

AppServiceTest::AppServiceTest() = default;

AppServiceTest::~AppServiceTest() = default;

void AppServiceTest::SetUp(Profile* profile) {
  app_service_ = std::make_unique<apps::AppService>(
      test_connector_factory_.RegisterInstance(apps::mojom::kServiceName));
  app_service_proxy_connector_ = test_connector_factory_.GetDefaultConnector();
  app_service_proxy_ = apps::AppServiceProxyFactory::GetForProfile(profile);
  DCHECK(app_service_proxy_);
  app_service_proxy_->ReInitializeForTesting(profile,
                                             app_service_proxy_connector_);

  // Allow async callbacks to run.
  WaitForAppService();
}

void AppServiceTest::WaitForAppService() {
  base::RunLoop().RunUntilIdle();
}

void AppServiceTest::FlushMojoCallsForAppService() {
  if (app_service_proxy_) {
    app_service_proxy_->FlushMojoCallsForTesting();
  }
}
