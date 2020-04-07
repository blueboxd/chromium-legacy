// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_TOKEN_CERT_DB_INITIALIZER_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_TOKEN_CERT_DB_INITIALIZER_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
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
class SystemTokenCertDBInitializer final : public CryptohomeClient::Observer {
 public:
  SystemTokenCertDBInitializer();
  ~SystemTokenCertDBInitializer() override;

  // Returns a global instance. May return null if not initialized.
  static SystemTokenCertDBInitializer* Get();

  // Stops making new requests to D-Bus services.
  void ShutDown();

  // CryptohomeClient::Observer:
  void TpmInitStatusUpdated(bool ready,
                            bool owned,
                            bool was_owned_this_boot) override;

  // Retrieves the global NSSCertDatabase for the system token and passes it to
  // |callback|. If the database is already initialized, calls |callback|
  // immediately. Otherwise, |callback| will be called when the database is
  // initialized.
  using GetSystemTokenCertDbCallback =
      base::OnceCallback<void(net::NSSCertDatabase*)>;
  void GetSystemTokenCertDb(GetSystemTokenCertDbCallback callback);

 private:
  // Called once the cryptohome service is available.
  void OnCryptohomeAvailable(bool available);

  // This is a callback for the cryptohome TpmIsReady query. Note that this is
  // not a listener which would be called once TPM becomes ready if it was not
  // ready on startup - that event is observed by TpmInitStatusUpdated().
  void OnGotTpmIsReady(base::Optional<bool> tpm_is_ready);

  // Starts loading the system slot and initializing the corresponding NSS cert
  // database, unless it was already started before.
  void MaybeStartInitializingDatabase();

  // This is called when the system token certificate database is created to run
  // all entries in |get_system_token_cert_db_callback_list_| and clear it.
  void RunAndClearGetSystemTokenCertDbCallbackList();

  // Initializes the global system token NSSCertDatabase with |system_slot|.
  // Also starts NetworkCertLoader with the system token database.
  void InitializeDatabase(crypto::ScopedPK11Slot system_slot);

  // Whether the database initialization was started.
  bool started_initializing_ = false;

  // Global NSSCertDatabase which sees the system token.
  std::unique_ptr<net::NSSCertDatabase> system_token_cert_database_;

  // List of callbacks that should be executed when the system token certificate
  // database is created.
  std::vector<GetSystemTokenCertDbCallback>
      get_system_token_cert_db_callback_list_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SystemTokenCertDBInitializer> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SystemTokenCertDBInitializer);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_TOKEN_CERT_DB_INITIALIZER_H_
