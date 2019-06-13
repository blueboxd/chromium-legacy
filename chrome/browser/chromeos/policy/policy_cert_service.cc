// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/policy_cert_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/post_task.h"
#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/cert/x509_certificate.h"
#include "services/network/nss_temp_certs_cache_chromeos.h"

namespace policy {

PolicyCertService::~PolicyCertService() {}

PolicyCertService::PolicyCertService(
    Profile* profile,
    const std::string& user_id,
    UserNetworkConfigurationUpdater* net_conf_updater,
    user_manager::UserManager* user_manager)
    : profile_(profile),
      user_id_(user_id),
      net_conf_updater_(net_conf_updater),
      user_manager_(user_manager),
      weak_ptr_factory_(this) {
  DCHECK(net_conf_updater_);
  DCHECK(user_manager_);
}

PolicyCertService::PolicyCertService(const std::string& user_id,
                                     user_manager::UserManager* user_manager)
    : user_id_(user_id),
      net_conf_updater_(NULL),
      user_manager_(user_manager),
      weak_ptr_factory_(this) {}

void PolicyCertService::StartObservingPolicyCerts() {
  // Don't notify the network service since it will get the initial list of
  // trust anchors in NetworkContextParams::initial_trust_anchors.
  StartObservingPolicyCertsInternal(false /* notify */);
}

void PolicyCertService::OnPolicyProvidedCertsChanged(
    const net::CertificateList& all_server_and_authority_certs,
    const net::CertificateList& trust_anchors) {
  OnPolicyProvidedCertsChangedInternal(all_server_and_authority_certs,
                                       trust_anchors, true /* notify */);
}

void PolicyCertService::StartObservingPolicyCertsInternal(bool notify) {
  net_conf_updater_->AddPolicyProvidedCertsObserver(this);

  // Set the current list of policy-provided server and authority certificates,
  // and the current list of trust anchors.
  net::CertificateList all_server_and_authority_certs =
      net_conf_updater_->GetAllServerAndAuthorityCertificates();
  net::CertificateList trust_anchors =
      net_conf_updater_->GetWebTrustedCertificates();
  OnPolicyProvidedCertsChangedInternal(all_server_and_authority_certs,
                                       trust_anchors, notify);
}

void PolicyCertService::OnPolicyProvidedCertsChangedInternal(
    const net::CertificateList& all_server_and_authority_certs,
    const net::CertificateList& trust_anchors,
    bool notify) {
  // Make all policy-provided server and authority certificates available to NSS
  // as temp certificates.
  // Note that this is done on the UI thread because the assumption is that NSS
  // has already been initialized by Chrome OS specific start-up code in chrome,
  // expecting that the operation of creating in-memory NSS certs is cheap in
  // that case.
  temp_policy_provided_certs_ =
      std::make_unique<network::NSSTempCertsCacheChromeOS>(
          all_server_and_authority_certs);
  all_server_and_authority_certs_ = all_server_and_authority_certs;

  // Do not use certificates installed via ONC policy if the current session has
  // multiple profiles. This is important to make sure that any possibly tainted
  // data is absolutely confined to the managed profile and never, ever leaks to
  // any other profile.
  if (!trust_anchors.empty() && user_manager_->GetLoggedInUsers().size() > 1u) {
    LOG(ERROR) << "Ignoring ONC-pushed certificates update because multiple "
               << "users are logged in.";
    trust_anchors_.clear();
  } else {
    trust_anchors_ = trust_anchors;
  }

  if (!notify)
    return;

  auto* profile_network_context =
      ProfileNetworkContextServiceFactory::GetForContext(profile_);
  if (profile_network_context) {  // null in unit tests.
    profile_network_context->UpdateAdditionalCertificates(
        all_server_and_authority_certs_, trust_anchors_);
  }
}

bool PolicyCertService::UsedPolicyCertificates() const {
  return PolicyCertServiceFactory::UsedPolicyCertificates(user_id_);
}

void PolicyCertService::Shutdown() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  if (net_conf_updater_)
    net_conf_updater_->RemovePolicyProvidedCertsObserver(this);
  OnPolicyProvidedCertsChanged(
      net::CertificateList() /* all_server_and_authority_certs */,
      net::CertificateList() /* trust_anchors */);
  net_conf_updater_ = NULL;
}

// static
std::unique_ptr<PolicyCertService> PolicyCertService::CreateForTesting(
    const std::string& user_id,
    user_manager::UserManager* user_manager) {
  return base::WrapUnique(new PolicyCertService(user_id, user_manager));
}

}  // namespace policy
