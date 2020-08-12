// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_ENCRYPTION_FAKE_ENCRYPTION_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_ENCRYPTION_FAKE_ENCRYPTION_H_

#include <string>

#include "base/callback.h"
#include "base/hash/hash.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/policy/messaging_layer/encryption/encryption.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"

namespace reporting {
namespace test {

// Fake implementation of EncryptorBase, intended for use in tests of
// reporting client.
// Record encryption with symmetric key is done by per-byte XOR.
// Key encryption with asymmetric public key is also done by per-byte XOR.
class FakeEncryptor : public EncryptorBase {
 public:
  // Factory method
  static StatusOr<scoped_refptr<EncryptorBase>> Create();

  void OpenRecord(base::OnceCallback<void(StatusOr<Handle*>)> cb) override;

 private:
  FakeEncryptor();
  ~FakeEncryptor() override;

  std::string EncryptSymmetricKey(base::StringPiece symmetric_key,
                                  base::StringPiece asymmetric_key) override;
};

}  // namespace test
}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_ENCRYPTION_FAKE_ENCRYPTION_H_
