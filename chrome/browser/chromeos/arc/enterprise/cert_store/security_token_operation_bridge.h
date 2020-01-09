// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ENTERPRISE_CERT_STORE_SECURITY_TOKEN_OPERATION_BRIDGE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ENTERPRISE_CERT_STORE_SECURITY_TOKEN_OPERATION_BRIDGE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/services/keymaster/public/mojom/cert_store.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/net_errors.h"

namespace chromeos {

class CertificateProviderService;

}  // namespace chromeos

namespace content {

class BrowserContext;

}  // namespace content

namespace arc {
namespace keymaster {

class SecurityTokenOperationBridge : public mojom::SecurityTokenOperation {
 public:
  SecurityTokenOperationBridge(
      content::BrowserContext* context,
      mojo::PendingReceiver<mojom::SecurityTokenOperation> receiver);

  // This constructor is used only for testing.
  explicit SecurityTokenOperationBridge(
      chromeos::CertificateProviderService* certificate_provider_service);
  SecurityTokenOperationBridge(const SecurityTokenOperationBridge&) = delete;
  SecurityTokenOperationBridge& operator=(const SecurityTokenOperationBridge&) =
      delete;
  ~SecurityTokenOperationBridge() override;

  // mojom::SecurityTokenOperation overrides.
  void SignDigest(const std::string& subject_public_key_info,
                  mojom::Algorithm algorithm,
                  mojom::Digest digest,
                  const std::vector<uint8_t>& data,
                  SignDigestCallback callback) override;

 private:
  void OnSignCompleted(SignDigestCallback callback,
                       net::Error error,
                       const std::vector<uint8_t>& signature);

  mojo::Receiver<mojom::SecurityTokenOperation> receiver_;

  chromeos::CertificateProviderService*
      certificate_provider_service_;  // Not owned.

  base::WeakPtrFactory<SecurityTokenOperationBridge> weak_ptr_factory_;
};

}  // namespace keymaster
}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ENTERPRISE_CERT_STORE_SECURITY_TOKEN_OPERATION_BRIDGE_H_
