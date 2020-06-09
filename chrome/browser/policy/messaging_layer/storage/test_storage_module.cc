// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/storage/test_storage_module.h"

#include <utility>

#include "base/callback.h"
#include "chrome/browser/policy/messaging_layer/public/report_queue.h"
#include "components/policy/proto/record.pb.h"
#include "components/policy/proto/record_constants.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace test {

using reporting::EncryptedRecord;
using reporting::Priority;

void TestStorageModule::AddRecord(EncryptedRecord record,
                                  Priority priority,
                                  base::OnceCallback<void(Status)> callback) {
  ASSERT_TRUE(
      wrapped_record_.ParseFromString(record.encrypted_wrapped_record()));
  priority_ = priority;
  std::move(callback).Run(Status::StatusOK());
}

void AlwaysFailsStorageModule::AddRecord(
    EncryptedRecord record,
    Priority priority,
    base::OnceCallback<void(Status)> callback) {
  std::move(callback).Run(Status(error::UNKNOWN, "Failing for Tests"));
}

}  // namespace test
}  // namespace reporting
