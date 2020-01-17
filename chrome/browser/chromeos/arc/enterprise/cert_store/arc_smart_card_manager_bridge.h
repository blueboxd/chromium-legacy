// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ENTERPRISE_CERT_STORE_ARC_SMART_CARD_MANAGER_BRIDGE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ENTERPRISE_CERT_STORE_ARC_SMART_CARD_MANAGER_BRIDGE_H_

#include <memory>
#include <set>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/arc/enterprise/cert_store/arc_cert_installer.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider.h"
#include "components/arc/mojom/cert_store.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "net/cert/scoped_nss_types.h"

class BrowserContextKeyedServiceFactory;

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

  // Return the factory instance for this class.
  static BrowserContextKeyedServiceFactory* GetFactory();

  ArcSmartCardManagerBridge(content::BrowserContext* context,
                            ArcBridgeService* bridge_service);

  // This constructor is public only for testing.
  ArcSmartCardManagerBridge(
      content::BrowserContext* context,
      ArcBridgeService* bridge_service,
      std::unique_ptr<chromeos::CertificateProvider> certificate_provider,
      std::unique_ptr<ArcCertInstaller> installer);

  ~ArcSmartCardManagerBridge() override;

  // SmartCardManagerHost overrides.
  void Refresh(RefreshCallback callback) override;

  // Returns a real SPKI for a dummy SPKI |dummy_spki|.
  // Returns empty string if the key is unknown.
  std::string GetRealSpkiForDummySpki(const std::string& dummy_spki);

  std::vector<std::string> get_required_cert_names() const {
    return certificate_cache_.get_required_cert_names();
  }

  void set_required_cert_names_for_testing(
      const std::vector<std::string>& cert_names) {
    certificate_cache_.set_required_cert_names_for_testing(
        std::set<std::string>(cert_names.begin(), cert_names.end()));
  }

 private:
  class CertificateCache {
   public:
    CertificateCache();
    ~CertificateCache();

    std::vector<net::ScopedCERTCertificate> Update(
        net::ClientCertIdentityList cert_identities);
    void Update(std::map<std::string, std::string> dummy_spki_by_name);
    std::string GetRealSpkiForDummySpki(const std::string& dummy_spki);

    bool need_policy_update() { return need_policy_update_; }
    void clear_need_policy_update() { need_policy_update_ = false; }
    std::vector<std::string> get_required_cert_names() const {
      return std::vector<std::string>(required_cert_names_.begin(),
                                      required_cert_names_.end());
    }
    void set_required_cert_names_for_testing(std::set<std::string> cert_names) {
      required_cert_names_ = std::move(cert_names);
    }

   private:
    bool need_policy_update_ = false;
    std::set<std::string> required_cert_names_;
    // Map dummy SPKI to real SPKI.
    std::map<std::string, std::string> real_spki_by_dummy_spki_cache_;
    // Map cert name to dummy SPKI.
    std::map<std::string, std::string> dummy_spki_by_name_cache_;
    // Intermediate map name to real SPKI.
    std::map<std::string, std::string> real_spki_by_name_cache_;
  };

  void DidGetCerts(RefreshCallback callback,
                   net::ClientCertIdentityList cert_identities);

  content::BrowserContext* const context_;
  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  std::unique_ptr<chromeos::CertificateProvider> certificate_provider_;
  std::unique_ptr<ArcCertInstaller> installer_;
  CertificateCache certificate_cache_;

  base::WeakPtrFactory<ArcSmartCardManagerBridge> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcSmartCardManagerBridge);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ENTERPRISE_CERT_STORE_ARC_SMART_CARD_MANAGER_BRIDGE_H_
