// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/enterprise/cert_store/arc_smart_card_manager_bridge.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chrome/browser/chromeos/arc/policy/arc_policy_bridge.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "chrome/common/net/x509_certificate_model_nss.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "crypto/rsa_private_key.h"
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

// static
BrowserContextKeyedServiceFactory* ArcSmartCardManagerBridge::GetFactory() {
  return ArcSmartCardManagerBridgeFactory::GetInstance();
}

ArcSmartCardManagerBridge::ArcSmartCardManagerBridge(
    content::BrowserContext* context,
    ArcBridgeService* bridge_service)
    : ArcSmartCardManagerBridge(
          context,
          bridge_service,
          chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
              context)
              ->CreateCertificateProvider(),
          std::make_unique<ArcCertInstaller>(context)) {}

ArcSmartCardManagerBridge::ArcSmartCardManagerBridge(
    content::BrowserContext* context,
    ArcBridgeService* bridge_service,
    std::unique_ptr<chromeos::CertificateProvider> certificate_provider,
    std::unique_ptr<ArcCertInstaller> installer)
    : context_(context),
      arc_bridge_service_(bridge_service),
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

std::string ArcSmartCardManagerBridge::GetRealSpkiForDummySpki(
    const std::string& dummy_spki) {
  return certificate_cache_.GetRealSpkiForDummySpki(dummy_spki);
}

ArcSmartCardManagerBridge::CertificateCache::CertificateCache() = default;
ArcSmartCardManagerBridge::CertificateCache::~CertificateCache() = default;

void ArcSmartCardManagerBridge::DidGetCerts(
    RefreshCallback callback,
    net::ClientCertIdentityList cert_identities) {
  VLOG(1) << "ArcSmartCardManagerBridge::DidGetCerts";

  certificate_cache_.clear_need_policy_update();
  std::vector<net::ScopedCERTCertificate> certificates =
      certificate_cache_.Update(std::move(cert_identities));

  // Maps cert name to dummy SPKI.
  std::map<std::string, std::string> installed_keys =
      installer_->InstallArcCerts(std::move(certificates), std::move(callback));

  certificate_cache_.Update(installed_keys);

  if (certificate_cache_.need_policy_update()) {
    ArcPolicyBridge* const policy_bridge =
        ArcPolicyBridge::GetForBrowserContext(context_);
    if (policy_bridge) {
      policy_bridge->OnPolicyUpdated(policy::PolicyNamespace(),
                                     policy::PolicyMap(), policy::PolicyMap());
    }
  }
}

std::vector<net::ScopedCERTCertificate>
ArcSmartCardManagerBridge::CertificateCache::Update(
    net::ClientCertIdentityList cert_identities) {
  std::vector<net::ScopedCERTCertificate> certificates;
  // Map cert name to real SPKI.
  real_spki_by_name_cache_.clear();
  std::set<std::string> new_required_cert_names;
  for (const auto& identity : cert_identities) {
    net::ScopedCERTCertificate nss_cert(
        net::x509_util::CreateCERTCertificateFromX509Certificate(
            identity->certificate()));
    if (!nss_cert) {
      LOG(ERROR) << "Certificate provider returned an invalid smart card "
                 << "certificate.";
      continue;
    }
    std::string cert_name =
        x509_certificate_model::GetCertNameOrNickname(nss_cert.get());
    real_spki_by_name_cache_[cert_name] =
        chromeos::platform_keys::GetSubjectPublicKeyInfo(
            identity->certificate());
    new_required_cert_names.insert(cert_name);

    certificates.push_back(std::move(nss_cert));
  }
  need_policy_update_ = (required_cert_names_ != new_required_cert_names);
  for (auto cert_name : required_cert_names_) {
    if (!new_required_cert_names.count(cert_name)) {
      real_spki_by_dummy_spki_cache_.erase(
          dummy_spki_by_name_cache_[cert_name]);
      dummy_spki_by_name_cache_.erase(cert_name);
    }
  }
  required_cert_names_ = new_required_cert_names;
  return certificates;
}

void ArcSmartCardManagerBridge::CertificateCache::Update(
    std::map<std::string, std::string> dummy_spki_by_name) {
  if (required_cert_names_.size() != dummy_spki_by_name.size())
    return;
  for (const auto& cert : dummy_spki_by_name) {
    const std::string& name = cert.first;
    if (!required_cert_names_.count(name)) {
      VLOG(1) << "ArcSmartCardManagerBridge::CertificateCache::Update error: "
              << "An attempt to add a non-required key " << name;
      continue;
    }

    std::string dummy_spki = cert.second;
    if (dummy_spki.empty() && dummy_spki_by_name_cache_.count(name))
      dummy_spki = dummy_spki_by_name_cache_[name];
    if (!dummy_spki.empty()) {
      dummy_spki_by_name_cache_[name] = dummy_spki;
      real_spki_by_dummy_spki_cache_[dummy_spki] =
          real_spki_by_name_cache_[name];
    }
  }
}

std::string
ArcSmartCardManagerBridge::CertificateCache::GetRealSpkiForDummySpki(
    const std::string& dummy_spki) {
  if (real_spki_by_dummy_spki_cache_.count(dummy_spki))
    return real_spki_by_dummy_spki_cache_[dummy_spki];
  return "";
}

}  // namespace arc
