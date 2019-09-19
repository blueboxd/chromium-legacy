// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_TOKEN_CERT_DB_INITIALIZER_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_TOKEN_CERT_DB_INITIALIZER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "crypto/scoped_nss_types.h"

namespace net {
class NSSCertDatabase;
}

namespace chromeos {

// Initializes a global NSSCertDatabase for the system token and starts
// NetworkCertLoader with that database.
//
// Lifetime: The global NetworkCertLoader instance must exist until ShutDown()
// has been called. The global NetworkCertLoader instance must exist until
// ShutDown() has been called, but must be outlived by this object.
//
// All of the methods must be called on the UI thread.
class SystemTokenCertDBInitializer final {
 public:
  SystemTokenCertDBInitializer();
  ~SystemTokenCertDBInitializer();

  // Stops making new requests to D-Bus services.
  void ShutDown();

 private:
  // Called once the cryptohome service is available.
  void OnCryptohomeAvailable(bool available);

  // This is a callback for the cryptohome TpmIsReady query. Note that this is
  // not a listener which would be called once TPM becomes ready if it was not
  // ready on startup (e.g. after device enrollment), see crbug.com/725500.
  void OnGotTpmIsReady(base::Optional<bool> tpm_is_ready);

  // Initializes the global system token NSSCertDatabase with |system_slot|.
  // Also starts NetworkCertLoader with the system token database.
  void InitializeDatabase(crypto::ScopedPK11Slot system_slot);

  // Global NSSCertDatabase which sees the system token.
  std::unique_ptr<net::NSSCertDatabase> system_token_cert_database_;

  base::WeakPtrFactory<SystemTokenCertDBInitializer> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SystemTokenCertDBInitializer);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_TOKEN_CERT_DB_INITIALIZER_H_
