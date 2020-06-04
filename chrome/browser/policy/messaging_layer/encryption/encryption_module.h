// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_ENCRYPTION_ENCRYPTION_MODULE_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_ENCRYPTION_ENCRYPTION_MODULE_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"

namespace reporting {

// TODO(b/153659559) Temporary EncryptionModule until the real one is ready.
class EncryptionModule : public base::RefCounted<EncryptionModule> {
 public:
  EncryptionModule() = default;

  EncryptionModule(const EncryptionModule& other) = delete;
  EncryptionModule& operator=(const EncryptionModule& other) = delete;

  // EncryptRecord will attempt to encrypt the provided |record|. On success the
  // return value will contain the encrypted string.
  virtual StatusOr<std::string> EncryptRecord(base::StringPiece record) const;

 protected:
  virtual ~EncryptionModule() = default;

 private:
  friend base::RefCounted<EncryptionModule>;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_ENCRYPTION_ENCRYPTION_MODULE_H_
