// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/policy/messaging_layer/encryption/test_encryption_module.h"

#include "chrome/browser/policy/messaging_layer/util/statusor.h"

namespace reporting {
namespace test {

StatusOr<std::string> TestEncryptionModule::EncryptRecord(
    base::StringPiece record) const {
  return std::string(record);
}

StatusOr<std::string> AlwaysFailsEncryptionModule::EncryptRecord(
    base::StringPiece record) const {
  return Status(error::UNKNOWN, "Failing for tests");
}

}  // namespace test
}  // namespace reporting
