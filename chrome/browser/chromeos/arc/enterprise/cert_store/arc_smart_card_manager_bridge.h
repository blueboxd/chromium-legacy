// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ENTERPRISE_CERT_STORE_ARC_SMART_CARD_MANAGER_BRIDGE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ENTERPRISE_CERT_STORE_ARC_SMART_CARD_MANAGER_BRIDGE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/arc/enterprise/cert_store/arc_cert_installer.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider.h"
#include "components/arc/mojom/cert_store.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "net/cert/scoped_nss_types.h"

namespace content {

class BrowserContext;

}  // namespace content

namespace arc {

class ArcBridgeService;

class ArcSmartCardManagerBridge : public KeyedService,
                                  public mojom::SmartCardManagerHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcSmartCardManagerBridge* GetForBrowserContext(
      content::BrowserContext* context);

  ArcSmartCardManagerBridge(content::BrowserContext* context,
                            ArcBridgeService* bridge_service);

  // This constructor is public only for testing.
  ArcSmartCardManagerBridge(
      ArcBridgeService* bridge_service,
      std::unique_ptr<chromeos::CertificateProvider> certificate_provider,
      std::unique_ptr<ArcCertInstaller> installer);

  ~ArcSmartCardManagerBridge() override;

  // SmartCardManagerHost overrides.
  void Refresh(RefreshCallback callback) override;

 private:
  void DidGetCerts(RefreshCallback callback,
                   net::ClientCertIdentityList cert_identities);

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  std::unique_ptr<chromeos::CertificateProvider> certificate_provider_;
  std::unique_ptr<ArcCertInstaller> installer_;

  base::WeakPtrFactory<ArcSmartCardManagerBridge> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcSmartCardManagerBridge);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ENTERPRISE_CERT_STORE_ARC_SMART_CARD_MANAGER_BRIDGE_H_
