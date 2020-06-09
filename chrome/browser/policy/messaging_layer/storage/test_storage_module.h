// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_STORAGE_TEST_STORAGE_MODULE_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_STORAGE_TEST_STORAGE_MODULE_H_

#include <utility>

#include "base/callback.h"
#include "chrome/browser/policy/messaging_layer/public/report_queue.h"
#include "components/policy/proto/record.pb.h"
#include "components/policy/proto/record_constants.pb.h"

namespace reporting {
namespace test {

// A |StorageModule| that stores the wrapped record and priority and calls the
// callback with an OK status.
class TestStorageModule : public StorageModule {
 public:
  TestStorageModule() = default;

  void AddRecord(reporting::EncryptedRecord record,
                 reporting::Priority priority,
                 base::OnceCallback<void(Status)> callback) override;

  reporting::WrappedRecord wrapped_record() { return wrapped_record_; }

  reporting::Priority priority() { return priority_; }

 protected:
  ~TestStorageModule() override = default;

 private:
  reporting::WrappedRecord wrapped_record_;
  reporting::Priority priority_;
};

// A |TestStorageModule| that always fails on |AddRecord| calls.
class AlwaysFailsStorageModule final : public TestStorageModule {
 public:
  AlwaysFailsStorageModule() = default;

  void AddRecord(reporting::EncryptedRecord record,
                 reporting::Priority priority,
                 base::OnceCallback<void(Status)> callback) override;

 protected:
  ~AlwaysFailsStorageModule() override = default;
};

}  // namespace test
}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_STORAGE_TEST_STORAGE_MODULE_H_
