// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/win/windows_system_proxy_resolution_service.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

// TaskEnvironment required to register an IP Address observer from
// ConfiguredProxyResolutionService.
using WindowsSystemProxyResolutionServiceTest = TestWithTaskEnvironment;

TEST_F(WindowsSystemProxyResolutionServiceTest,
       CastToConfiguredProxyResolutionService) {
  WindowsSystemProxyResolutionService service;

  auto configured_service = ConfiguredProxyResolutionService::CreateDirect();
  ConfiguredProxyResolutionService* casted_service = configured_service.get();
  EXPECT_FALSE(service.CastToConfiguredProxyResolutionService(&casted_service));
  EXPECT_EQ(nullptr, casted_service);
}

}  // namespace net
