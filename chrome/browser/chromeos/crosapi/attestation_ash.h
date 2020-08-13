// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSAPI_ATTESTATION_ASH_H_
#define CHROME_BROWSER_CHROMEOS_CROSAPI_ATTESTATION_ASH_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/attestation.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {
namespace attestation {
class TpmChallengeKey;
struct TpmChallengeKeyResult;
}  // namespace attestation
}  // namespace chromeos

namespace crosapi {

// This class is the ash implementation of the Attestation crosapi. It allows
// lacros to expose blessed extension APIs which issue key challenges. These in
// turn are forwarded to ash, which signs the challenge with a private key.
class AttestationAsh : public mojom::Attestation {
 public:
  explicit AttestationAsh(mojo::PendingReceiver<mojom::Attestation> receiver);
  AttestationAsh(const AttestationAsh&) = delete;
  AttestationAsh& operator=(const AttestationAsh&) = delete;
  ~AttestationAsh() override;

  // mojom::Attestation:
  void ChallengeKey(const std::string& challenge,
                    mojom::ChallengeKeyType type,
                    ChallengeKeyCallback callback) override;

 private:
  // |challenge| is used as a opaque identifier to match against the unique_ptr
  // in outstanding_challenges_. It should not be dereferenced.
  void DidChallengeKey(
      ChallengeKeyCallback callback,
      void* challenge,
      const chromeos::attestation::TpmChallengeKeyResult& result);

  // Container to keep outstanding challenges alive.
  std::vector<std::unique_ptr<chromeos::attestation::TpmChallengeKey>>
      outstanding_challenges_;
  mojo::Receiver<mojom::Attestation> receiver_;

  base::WeakPtrFactory<AttestationAsh> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_CHROMEOS_CROSAPI_ATTESTATION_ASH_H_
