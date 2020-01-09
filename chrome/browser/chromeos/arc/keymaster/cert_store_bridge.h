// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_KEYMASTER_CERT_STORE_BRIDGE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_KEYMASTER_CERT_STORE_BRIDGE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/arc/enterprise/cert_store/security_token_operation_bridge.h"
#include "chrome/services/keymaster/public/mojom/cert_store.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/invitation.h"

namespace content {

class BrowserContext;

}  // namespace content

namespace arc {
namespace keymaster {

class CertStoreBridge : public mojom::CertStoreHost {
 public:
  explicit CertStoreBridge(content::BrowserContext* context);
  CertStoreBridge(const CertStoreBridge&) = delete;
  CertStoreBridge& operator=(const CertStoreBridge&) = delete;
  ~CertStoreBridge() override;

  // Attaches a new message pipe to the invitation and binds it to the cert
  // store instance proxy.
  void BindToInvitation(mojo::OutgoingInvitation* invitation);
  void OnBootstrapMojoConnection(bool result);

  // CertStoreHost overrides.
  void GetSecurityTokenOperation(
      mojom::SecurityTokenOperationRequest operation_request,
      GetSecurityTokenOperationCallback callback) override;

 private:
  void OnConnectionReady(
      std::unique_ptr<mojo::Binding<mojom::CertStoreHost>> binding);
  void OnConnectionClosed();

  content::BrowserContext* context_;  // not owned.

  // Points to a proxy bound to the implementation in arc-keymasterd.
  keymaster::mojom::CertStoreInstancePtr cert_store_proxy_;

  std::unique_ptr<SecurityTokenOperationBridge> security_token_operation_;

  std::unique_ptr<mojo::Binding<mojom::CertStoreHost>> binding_;

  base::WeakPtrFactory<CertStoreBridge> weak_ptr_factory_;
};

}  // namespace keymaster
}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_KEYMASTER_CERT_STORE_BRIDGE_H_
