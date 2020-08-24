// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_ENCRYPTION_FAKE_DECRYPTION_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_ENCRYPTION_FAKE_DECRYPTION_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/policy/messaging_layer/encryption/decryption.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"

namespace reporting {
namespace test {

// Fake implementation of DecryptorBase, intended for use in tests of
// reporting client.
// Key decryption with asymmetric private key is done by per-byte XOR in reverse
// order: public and private key are reverse, so if the encryption used XOR with
// public key "012", decryption will use private key "210" and XOR will be from
// the last to the first bytes.
// Record decryption with symmetric key is done by per-byte XOR.
class FakeDecryptor : public DecryptorBase {
 public:
  // Factory method
  static StatusOr<scoped_refptr<DecryptorBase>> Create();

  void OpenRecord(base::StringPiece encrypted_key,
                  base::OnceCallback<void(StatusOr<Handle*>)> cb) override;

  StatusOr<std::string> DecryptKey(base::StringPiece private_key,
                                   base::StringPiece encrypted_key) override;

 private:
  FakeDecryptor();
  ~FakeDecryptor() override;
};

}  // namespace test
}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_ENCRYPTION_FAKE_DECRYPTION_H_
