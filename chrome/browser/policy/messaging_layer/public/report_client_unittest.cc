// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/public/report_client.h"

#include "base/memory/singleton.h"
#include "base/synchronization/waitable_event.h"
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

class TestCallbackWaiter {
 public:
  TestCallbackWaiter()
      : completed_(base::WaitableEvent::ResetPolicy::MANUAL,
                   base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  virtual void Signal() {
    DCHECK(!completed_.IsSignaled());
    completed_.Signal();
  }

  void Wait() { completed_.Wait(); }
  void Reset() { completed_.Reset(); }

 protected:
  base::WaitableEvent completed_;
};

class ReportingClientTest : public testing::Test {
 public:
  void TearDown() override { ReportingClient::Reset_test(); }

 protected:
  base::test::TaskEnvironment task_envrionment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
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
  ASSERT_OK(config_result);

  TestCallbackWaiter waiter;
  StatusOr<std::unique_ptr<ReportQueue>> result;
  auto create_report_queue_cb = base::BindOnce(
      [](TestCallbackWaiter* waiter,
         StatusOr<std::unique_ptr<ReportQueue>>* result,
         StatusOr<std::unique_ptr<ReportQueue>> create_result) {
        *result = std::move(create_result);
        waiter->Signal();
      },
      &waiter, &result);
  ReportingClient::CreateReportQueue(std::move(config_result.ValueOrDie()),
                                     std::move(create_report_queue_cb));

  waiter.Wait();
  waiter.Reset();
  ASSERT_OK(result);
}

// Ensures that created ReportQueues are actually different.
TEST_F(ReportingClientTest, CreatesTwoDifferentReportQueues) {
  auto config_result = ReportQueueConfiguration::Create(
      dm_token_, destination_, priority_, policy_checker_callback_);
  EXPECT_TRUE(config_result.ok());

  TestCallbackWaiter waiter;
  StatusOr<std::unique_ptr<ReportQueue>> result;
  auto create_report_queue_cb = base::BindOnce(
      [](TestCallbackWaiter* waiter,
         StatusOr<std::unique_ptr<ReportQueue>>* result,
         StatusOr<std::unique_ptr<ReportQueue>> create_result) {
        *result = std::move(create_result);
        waiter->Signal();
      },
      &waiter, &result);
  ReportingClient::CreateReportQueue(std::move(config_result.ValueOrDie()),
                                     std::move(create_report_queue_cb));
  waiter.Wait();
  waiter.Reset();
  ASSERT_OK(result);
  auto report_queue_1 = std::move(result.ValueOrDie());

  config_result = ReportQueueConfiguration::Create(
      dm_token_, destination_, priority_, policy_checker_callback_);
  create_report_queue_cb = base::BindOnce(
      [](TestCallbackWaiter* waiter,
         StatusOr<std::unique_ptr<ReportQueue>>* result,
         StatusOr<std::unique_ptr<ReportQueue>> create_result) {
        *result = std::move(create_result);
        waiter->Signal();
      },
      &waiter, &result);
  ReportingClient::CreateReportQueue(std::move(config_result.ValueOrDie()),
                                     std::move(create_report_queue_cb));
  waiter.Wait();
  ASSERT_OK(result);

  auto report_queue_2 = std::move(result.ValueOrDie());

  EXPECT_NE(report_queue_1.get(), report_queue_2.get());
}

}  // namespace
}  // namespace reporting
