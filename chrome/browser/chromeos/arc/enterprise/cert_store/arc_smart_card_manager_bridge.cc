// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/enterprise/cert_store/arc_smart_card_manager_bridge.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service_factory.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "net/cert/x509_util_nss.h"

namespace arc {

namespace {

// Singleton factory for ArcSmartCardManagerBridge.
class ArcSmartCardManagerBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcSmartCardManagerBridge,
          ArcSmartCardManagerBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcSmartCardManagerBridgeFactory";

  static ArcSmartCardManagerBridgeFactory* GetInstance() {
    return base::Singleton<ArcSmartCardManagerBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcSmartCardManagerBridgeFactory>;
  ArcSmartCardManagerBridgeFactory() {
    DependsOn(chromeos::CertificateProviderServiceFactory::GetInstance());
  }

  ~ArcSmartCardManagerBridgeFactory() override = default;
};

}  // namespace

// static
ArcSmartCardManagerBridge* ArcSmartCardManagerBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcSmartCardManagerBridgeFactory::GetForBrowserContext(context);
}

ArcSmartCardManagerBridge::ArcSmartCardManagerBridge(
    content::BrowserContext* context,
    ArcBridgeService* bridge_service)
    : ArcSmartCardManagerBridge(
          bridge_service,
          chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
              context)
              ->CreateCertificateProvider(),
          std::make_unique<ArcCertInstaller>(context)) {}

ArcSmartCardManagerBridge::ArcSmartCardManagerBridge(
    ArcBridgeService* bridge_service,
    std::unique_ptr<chromeos::CertificateProvider> certificate_provider,
    std::unique_ptr<ArcCertInstaller> installer)
    : arc_bridge_service_(bridge_service),
      certificate_provider_(std::move(certificate_provider)),
      installer_(std::move(installer)),
      weak_ptr_factory_(this) {
  VLOG(1) << "ArcSmartCardManagerBridge::ArcSmartCardManagerBridge";
  arc_bridge_service_->smart_card_manager()->SetHost(this);
}

ArcSmartCardManagerBridge::~ArcSmartCardManagerBridge() {
  VLOG(1) << "ArcSmartCardManagerBridge::~ArcSmartCardManagerBridge";

  arc_bridge_service_->smart_card_manager()->SetHost(nullptr);
}

void ArcSmartCardManagerBridge::Refresh(RefreshCallback callback) {
  VLOG(1) << "ArcSmartCardManagerBridge::Refresh";

  certificate_provider_->GetCertificates(
      base::BindOnce(&ArcSmartCardManagerBridge::DidGetCerts,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcSmartCardManagerBridge::DidGetCerts(
    RefreshCallback callback,
    net::ClientCertIdentityList cert_identities) {
  VLOG(1) << "ArcSmartCardManagerBridge::DidGetCerts";

  std::vector<net::ScopedCERTCertificate> certificates;
  for (const auto& identity : cert_identities) {
    net::ScopedCERTCertificate nss_cert(
        net::x509_util::CreateCERTCertificateFromX509Certificate(
            identity->certificate()));
    if (!nss_cert) {
      LOG(ERROR) << "Certificate provider returned an invalid smart card "
                 << "certificate.";
      continue;
    }

    certificates.push_back(std::move(nss_cert));
  }
  installer_->InstallArcCerts(std::move(certificates), std::move(callback));
}

}  // namespace arc
