// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/attestation_ash.h"

#include <utility>

#include "chrome/browser/chromeos/attestation/tpm_challenge_key.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace crosapi {

AttestationAsh::AttestationAsh(
    mojo::PendingReceiver<mojom::Attestation> receiver)
    : receiver_(this, std::move(receiver)) {}

AttestationAsh::~AttestationAsh() = default;

void AttestationAsh::ChallengeKey(const std::string& challenge,
                                  mojom::ChallengeKeyType type,
                                  ChallengeKeyCallback callback) {
  if (!crosapi::mojom::IsKnownEnumValue(type)) {
    mojom::ChallengeKeyResultPtr result_ptr = mojom::ChallengeKeyResult::New();
    result_ptr->set_error_message("unsupported challenge key type");
    std::move(callback).Run(std::move(result_ptr));
    return;
  }
  chromeos::attestation::AttestationKeyType key_type;
  switch (type) {
    case mojom::ChallengeKeyType::kUser:
      key_type = chromeos::attestation::KEY_USER;
      break;
    case mojom::ChallengeKeyType::kDevice:
      key_type = chromeos::attestation::KEY_DEVICE;
      break;
  }
  Profile* profile = ProfileManager::GetActiveUserProfile();

  std::unique_ptr<chromeos::attestation::TpmChallengeKey> challenge_key =
      chromeos::attestation::TpmChallengeKeyFactory::Create();
  chromeos::attestation::TpmChallengeKey* challenge_key_ptr =
      challenge_key.get();
  outstanding_challenges_.push_back(std::move(challenge_key));
  challenge_key_ptr->BuildResponse(
      key_type, profile,
      base::BindOnce(&AttestationAsh::DidChallengeKey,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     challenge_key_ptr),
      challenge,
      /*register_key=*/false, /*key_name_for_spkac=*/"");
}

void AttestationAsh::DidChallengeKey(
    ChallengeKeyCallback callback,
    void* challenge_key_ptr,
    const chromeos::attestation::TpmChallengeKeyResult& result) {
  mojom::ChallengeKeyResultPtr result_ptr = mojom::ChallengeKeyResult::New();
  if (result.IsSuccess()) {
    result_ptr->set_challenge_response(result.challenge_response);
  } else {
    result_ptr->set_error_message(result.GetErrorMessage());
  }
  std::move(callback).Run(std::move(result_ptr));

  // Remove the outstanding challenge_key object.
  bool found = false;
  for (auto it = outstanding_challenges_.begin();
       it != outstanding_challenges_.end(); ++it) {
    if (it->get() == challenge_key_ptr) {
      outstanding_challenges_.erase(it);
      found = true;
      break;
    }
  }
  DCHECK(found);
}

}  // namespace crosapi
