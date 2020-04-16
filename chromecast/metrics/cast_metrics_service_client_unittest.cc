// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/metrics/cast_metrics_service_client.h"

#include "base/test/task_environment.h"
#include "chromecast/metrics/mock_cast_sys_info_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

class CastMetricsServiceClientTest : public testing::Test {
 public:
  CastMetricsServiceClientTest() {}
  CastMetricsServiceClientTest(const CastMetricsServiceClientTest&) = delete;
  CastMetricsServiceClientTest& operator=(const CastMetricsServiceClientTest&) =
      delete;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

/**
 * Validates that class instatiation and calls to GetChannel() only
 * result in a single method invocation of CreateSysInfo().
 */
TEST_F(CastMetricsServiceClientTest, CreateSysInfoSingleInvocation) {
  EXPECT_EQ(chromecast::GetSysInfoCreatedCount(), 0);

  chromecast::metrics::CastMetricsServiceClient metrics_client(nullptr, nullptr,
                                                               nullptr);

  metrics_client.GetChannel();
  metrics_client.GetChannel();

  // Despite muiltiple calls to GetChannel(),
  // SysInfo should only be created a single time
  EXPECT_EQ(chromecast::GetSysInfoCreatedCount(), 1);
}