// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/encryption/encryption_module.h"

#include "base/callback.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "components/policy/proto/record.pb.h"

namespace reporting {

void EncryptionModule::EncryptRecord(
    base::StringPiece record,
    base::OnceCallback<void(StatusOr<EncryptedRecord>)> cb) const {
  std::move(cb).Run(
      Status(error::UNIMPLEMENTED, "EncryptRecord isn't implemented"));
}

}  // namespace reporting
