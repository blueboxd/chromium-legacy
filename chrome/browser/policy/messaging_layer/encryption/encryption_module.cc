// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/policy/messaging_layer/encryption/encryption_module.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"

namespace reporting {

// EncryptRecord will attempt to encrypt the provided |record|. On success the
// return value will contain the encrypted string.
StatusOr<std::string> EncryptionModule::EncryptRecord(
    base::StringPiece record) const {
  return Status(error::UNIMPLEMENTED, "EncryptRecord isn't implemented");
}

}  // namespace reporting
