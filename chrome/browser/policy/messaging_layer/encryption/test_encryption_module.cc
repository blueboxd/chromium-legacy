// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/policy/messaging_layer/encryption/test_encryption_module.h"

#include "chrome/browser/policy/messaging_layer/util/statusor.h"

using ::testing::Invoke;

namespace reporting {
namespace test {

TestEncryptionModule::TestEncryptionModule() {
  ON_CALL(*this, EncryptRecord)
      .WillByDefault(
          Invoke([](base::StringPiece record) { return std::string(record); }));
}

TestEncryptionModule::~TestEncryptionModule() = default;

}  // namespace test
}  // namespace reporting
