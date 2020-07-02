// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/storage/test_storage_module.h"

#include <utility>

#include "base/callback.h"
#include "chrome/browser/policy/messaging_layer/public/report_queue.h"
#include "components/policy/proto/record.pb.h"
#include "components/policy/proto/record_constants.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Invoke;

namespace reporting {
namespace test {

TestStorageModule::TestStorageModule() {
  ON_CALL(*this, AddRecord)
      .WillByDefault(Invoke(this, &TestStorageModule::AddRecordSuccessfully));
}

TestStorageModule::~TestStorageModule() = default;

WrappedRecord TestStorageModule::wrapped_record() const {
  EXPECT_TRUE(wrapped_record_.has_value());
  return wrapped_record_.value();
}

Priority TestStorageModule::priority() const {
  EXPECT_TRUE(priority_.has_value());
  return priority_.value();
}

void TestStorageModule::AddRecordSuccessfully(
    EncryptedRecord record,
    Priority priority,
    base::OnceCallback<void(Status)> callback) {
  WrappedRecord wrapped_record;
  ASSERT_TRUE(
      wrapped_record.ParseFromString(record.encrypted_wrapped_record()));
  wrapped_record_ = wrapped_record;
  priority_ = priority;
  std::move(callback).Run(Status::StatusOK());
}
}  // namespace test
}  // namespace reporting
