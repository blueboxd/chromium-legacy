// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/public/report_queue.h"

#include <stdio.h>

#include <utility>

#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/policy/messaging_layer/encryption/encryption_module.h"
#include "chrome/browser/policy/messaging_layer/encryption/test_encryption_module.h"
#include "chrome/browser/policy/messaging_layer/proto/test.pb.h"
#include "chrome/browser/policy/messaging_layer/public/report_queue_configuration.h"
#include "chrome/browser/policy/messaging_layer/storage/storage_module.h"
#include "chrome/browser/policy/messaging_layer/storage/test_storage_module.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/status_macros.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/proto/record_constants.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {

using PolicyCheckCallback =
    reporting::ReportQueueConfiguration::PolicyCheckCallback;

using base::MakeRefCounted;
using policy::DMToken;
using reporting::test::AlwaysFailsEncryptionModule;
using reporting::test::AlwaysFailsStorageModule;
using reporting::test::TestEncryptionModule;
using reporting::test::TestStorageModule;

// Creates a |ReportQueue| using |TestStorageModule| and |TestEncryptionModule|.
// Allows access to the storage module for checking stored values.
class ReportQueueTest : public testing::Test {
 public:
  ReportQueueTest()
      : completed_(base::WaitableEvent::ResetPolicy::MANUAL,
                   base::WaitableEvent::InitialState::NOT_SIGNALED),
        result_(error::INTERNAL, "initialized with non-ok status"),
        storage_module_(MakeRefCounted<TestStorageModule>()),
        priority_(Priority::IMMEDIATE),
        dm_token_(DMToken::CreateValidTokenForTesting("FAKE_DM_TOKEN")),
        destination_(Destination::UPLOAD_EVENTS),
        encryption_module_(MakeRefCounted<TestEncryptionModule>()),
        policy_check_callback_(base::BindRepeating(
            []() -> Status { return Status::StatusOK(); })) {}

  // Allows specifying an alternative |TestStorageModule| for testing different
  // cases.
  explicit ReportQueueTest(scoped_refptr<TestStorageModule> storage_module)
      : completed_(base::WaitableEvent::ResetPolicy::MANUAL,
                   base::WaitableEvent::InitialState::NOT_SIGNALED),
        result_(error::INTERNAL, "initialized with non-ok status"),
        storage_module_(storage_module),
        priority_(Priority::IMMEDIATE),
        dm_token_(DMToken::CreateValidTokenForTesting("FAKE_DM_TOKEN")),
        destination_(Destination::UPLOAD_EVENTS),
        encryption_module_(MakeRefCounted<TestEncryptionModule>()),
        policy_check_callback_(base::BindRepeating(
            []() -> Status { return Status::StatusOK(); })) {}

  // Allows specifying an alternative |TestEncryptionModule| for testing
  // different cases.
  explicit ReportQueueTest(
      scoped_refptr<TestEncryptionModule> encryption_module)
      : completed_(base::WaitableEvent::ResetPolicy::MANUAL,
                   base::WaitableEvent::InitialState::NOT_SIGNALED),
        result_(error::INTERNAL, "initialized with non-ok status"),
        storage_module_(MakeRefCounted<TestStorageModule>()),
        priority_(Priority::IMMEDIATE),
        dm_token_(DMToken::CreateValidTokenForTesting("FAKE_DM_TOKEN")),
        destination_(Destination::UPLOAD_EVENTS),
        encryption_module_(encryption_module),
        policy_check_callback_(base::BindRepeating(
            []() -> Status { return Status::StatusOK(); })) {}

  explicit ReportQueueTest(
      ReportQueueConfiguration::PolicyCheckCallback policy_check_callback)
      : completed_(base::WaitableEvent::ResetPolicy::MANUAL,
                   base::WaitableEvent::InitialState::NOT_SIGNALED),
        result_(error::INTERNAL, "initialized with non-ok status"),
        storage_module_(MakeRefCounted<TestStorageModule>()),
        priority_(Priority::IMMEDIATE),
        dm_token_(DMToken::CreateValidTokenForTesting("FAKE_DM_TOKEN")),
        destination_(Destination::UPLOAD_EVENTS),
        encryption_module_(MakeRefCounted<TestEncryptionModule>()),
        policy_check_callback_(std::move(policy_check_callback)) {}

  void SetUp() override {
    StatusOr<std::unique_ptr<ReportQueueConfiguration>> config_result =
        ReportQueueConfiguration::Create(dm_token_, destination_, priority_,
                                         policy_check_callback_);

    ASSERT_TRUE(config_result.ok());

    StatusOr<std::unique_ptr<ReportQueue>> report_queue_result =
        ReportQueue::Create(std::move(config_result.ValueOrDie()),
                            storage_module_, encryption_module_);

    ASSERT_TRUE(report_queue_result.ok());

    report_queue_ = std::move(report_queue_result.ValueOrDie());

    callback_ = base::BindOnce(
        [](base::WaitableEvent* completed, Status* result,
           Status status) -> void {
          *result = status;
          completed->Signal();
        },
        &completed_, &result_);
  }

 protected:
  base::WaitableEvent completed_;
  Status result_;
  base::test::TaskEnvironment task_envrionment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  scoped_refptr<TestStorageModule> storage_module_;
  const Priority priority_;

  std::unique_ptr<ReportQueue> report_queue_;
  base::OnceCallback<void(Status)> callback_;

 private:
  const DMToken dm_token_;
  const Destination destination_;
  scoped_refptr<TestEncryptionModule> encryption_module_;
  PolicyCheckCallback policy_check_callback_;
};

// Enqueues a random string and ensures that the string arrives unaltered in the
// |StorageModule|.
TEST_F(ReportQueueTest, SuccessfulStringRecord) {
  constexpr char kTestString[] = "El-Chupacabra";
  Status status = report_queue_->Enqueue(kTestString, std::move(callback_));
  ASSERT_TRUE(status.ok());

  completed_.Wait();

  EXPECT_TRUE(result_.ok());

  EXPECT_EQ(storage_module_->priority(), priority_);

  EXPECT_EQ(storage_module_->wrapped_record().record().data(), kTestString);
}

// Enqueues a |base::Value| dictionary and ensures it arrives unaltered in the
// |StorageModule|.
TEST_F(ReportQueueTest, SuccessfulBaseValueRecord) {
  constexpr char kTestKey[] = "TEST_KEY";
  constexpr char kTestValue[] = "TEST_VALUE";
  base::Value test_dict(base::Value::Type::DICTIONARY);
  test_dict.SetStringKey(kTestKey, kTestValue);
  Status status = report_queue_->Enqueue(test_dict, std::move(callback_));
  ASSERT_TRUE(status.ok());

  completed_.Wait();

  EXPECT_TRUE(result_.ok());

  EXPECT_EQ(storage_module_->priority(), priority_);

  base::Optional<base::Value> value_result =
      base::JSONReader::Read(storage_module_->wrapped_record().record().data());
  ASSERT_TRUE(value_result);
  EXPECT_EQ(value_result.value(), test_dict);
}

// Enqueues a |TestMessage| and ensures that it arrives unaltered in the
// |StorageModule|.
TEST_F(ReportQueueTest, SuccessfulProtoRecord) {
  reporting::test::TestMessage test_message;
  test_message.set_test("TEST_MESSAGE");
  Status status = report_queue_->Enqueue(&test_message, std::move(callback_));
  ASSERT_TRUE(status.ok());

  completed_.Wait();

  EXPECT_TRUE(result_.ok());

  EXPECT_EQ(storage_module_->priority(), priority_);

  reporting::test::TestMessage result_message;
  ASSERT_TRUE(result_message.ParseFromString(
      storage_module_->wrapped_record().record().data()));
  ASSERT_EQ(result_message.test(), test_message.test());
}

// A |ReportQueueTest| built with an |AlwaysFailsStorageModule|.
class StorageFailsReportQueueTest : public ReportQueueTest {
 public:
  StorageFailsReportQueueTest()
      : ReportQueueTest(MakeRefCounted<AlwaysFailsStorageModule>()) {}
};

// The call to enqueue should succeed, indicating that the storage operation has
// been scheduled. The callback should fail, indicating that storage was
// unsuccessful.
TEST_F(StorageFailsReportQueueTest, CallSuccessCallbackFailure) {
  reporting::test::TestMessage test_message;
  test_message.set_test("TEST_MESSAGE");
  Status status = report_queue_->Enqueue(&test_message, std::move(callback_));
  ASSERT_TRUE(status.ok());

  completed_.Wait();

  EXPECT_FALSE(result_.ok());
  EXPECT_EQ(result_.error_code(), error::UNKNOWN);
}

// A |ReportQueueTest| built with an |AlwaysFailsEncryptionModule|.
class EncryptionFailsReportQueueTest : public ReportQueueTest {
 public:
  EncryptionFailsReportQueueTest()
      : ReportQueueTest(MakeRefCounted<AlwaysFailsEncryptionModule>()) {}
};

// The call to enqueue should succeed, indicating that the encryption operation
// has been scheduled. The callback should fail, indicating that encryption was
// unsuccessful.
TEST_F(EncryptionFailsReportQueueTest, CallSuccessCallFailure) {
  reporting::test::TestMessage test_message;
  test_message.set_test("TEST_MESSAGE");
  Status status = report_queue_->Enqueue(&test_message, std::move(callback_));
  ASSERT_TRUE(status.ok());

  completed_.Wait();

  EXPECT_FALSE(result_.ok());
  EXPECT_EQ(result_.error_code(), error::UNKNOWN);
}

class ReportQueueRespectsPolicyTest : public ReportQueueTest {
 public:
  ReportQueueRespectsPolicyTest()
      : ReportQueueTest(base::BindRepeating([]() -> Status {
          return Status(error::UNAUTHENTICATED, "Failing for tests");
        })) {}
};

TEST_F(ReportQueueRespectsPolicyTest, EnqueueStringFailsOnPolicy) {
  constexpr char kTestString[] = "El-Chupacabra";
  Status status = report_queue_->Enqueue(kTestString, std::move(callback_));
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.error_code(), error::UNAUTHENTICATED);
}

TEST_F(ReportQueueRespectsPolicyTest, EnqueueProtoFailsOnPolicy) {
  reporting::test::TestMessage test_message;
  test_message.set_test("TEST_MESSAGE");
  Status status = report_queue_->Enqueue(&test_message, std::move(callback_));
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.error_code(), error::UNAUTHENTICATED);
}

TEST_F(ReportQueueRespectsPolicyTest, EnqueueValueFailsOnPolicy) {
  constexpr char kTestKey[] = "TEST_KEY";
  constexpr char kTestValue[] = "TEST_VALUE";
  base::Value test_dict(base::Value::Type::DICTIONARY);
  test_dict.SetStringKey(kTestKey, kTestValue);
  Status status = report_queue_->Enqueue(test_dict, std::move(callback_));
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.error_code(), error::UNAUTHENTICATED);
}

}  // namespace
}  // namespace reporting
