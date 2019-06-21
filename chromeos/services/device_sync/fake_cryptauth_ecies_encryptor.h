// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_ECIES_ENCRYPTOR_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_ECIES_ENCRYPTOR_H_

#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/services/device_sync/cryptauth_ecies_encryptor.h"

namespace chromeos {

namespace device_sync {

class FakeCryptAuthEciesEncryptor : CryptAuthEciesEncryptor {
 public:
  enum class Action { kUndefined, kEncryption, kDecryption };

  FakeCryptAuthEciesEncryptor();
  ~FakeCryptAuthEciesEncryptor() override;

  void FinishAttempt(Action expected_action,
                     const IdToOutputMap& id_to_output_map);

  const IdToInputMap& id_to_input_map() const { return id_to_input_map_; }

 private:
  // CryptAuthEciesEncryptor:
  void OnBatchEncryptionStarted() override;
  void OnBatchDecryptionStarted() override;

  Action action_ = Action::kUndefined;

  DISALLOW_COPY_AND_ASSIGN(FakeCryptAuthEciesEncryptor);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_ECIES_ENCRYPTOR_H_
