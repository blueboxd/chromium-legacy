// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/public/report_client.h"

#include "base/test/task_environment.h"
#include "chrome/browser/policy/messaging_layer/public/report_queue.h"
#include "chrome/browser/policy/messaging_layer/public/report_queue_configuration.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/status_macros.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/proto/record_constants.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {

using policy::DMToken;
using reporting::Destination;
using reporting::Priority;

class ReportingClientTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_envrionment_;

  const DMToken dm_token_ = DMToken::CreateValidTokenForTesting("TOKEN");
  const Destination destination_ = Destination::UPLOAD_EVENTS;
  const Priority priority_ = Priority::IMMEDIATE;
  ReportQueueConfiguration::PolicyCheckCallback policy_checker_callback_ =
      base::BindRepeating([]() { return Status::StatusOK(); });
};

// Tests that a ReportQueue can be created using the ReportingClient.
TEST_F(ReportingClientTest, CreatesReportQueue) {
  auto config_result = ReportQueueConfiguration::Create(
      dm_token_, destination_, priority_, policy_checker_callback_);
  EXPECT_TRUE(config_result.ok());

  auto report_queue_result =
      ReportingClient::CreateReportQueue(std::move(config_result.ValueOrDie()));

  EXPECT_TRUE(report_queue_result.ok());
}

// Ensures that created ReportQueues are actually different.
TEST_F(ReportingClientTest, CreatesTwoDifferentReportQueues) {
  auto config_result = ReportQueueConfiguration::Create(
      dm_token_, destination_, priority_, policy_checker_callback_);
  EXPECT_TRUE(config_result.ok());

  auto report_queue_result_1 =
      ReportingClient::CreateReportQueue(std::move(config_result.ValueOrDie()));
  EXPECT_TRUE(report_queue_result_1.ok());

  config_result = ReportQueueConfiguration::Create(
      dm_token_, destination_, priority_, policy_checker_callback_);
  EXPECT_TRUE(config_result.ok());

  auto report_queue_result_2 =
      ReportingClient::CreateReportQueue(std::move(config_result.ValueOrDie()));
  EXPECT_TRUE(report_queue_result_2.ok());

  EXPECT_NE(report_queue_result_1.ValueOrDie().get(),
            report_queue_result_2.ValueOrDie().get());
}

}  // namespace
}  // namespace reporting
