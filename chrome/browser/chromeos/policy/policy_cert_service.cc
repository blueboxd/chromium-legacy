// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/policy_cert_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/post_task.h"
#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"
#include "chrome/browser/chromeos/policy/user_network_configuration_updater.h"
#include "chrome/browser/chromeos/policy/user_network_configuration_updater_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/cert/x509_certificate.h"
#include "services/network/nss_temp_certs_cache_chromeos.h"

namespace policy {

PolicyCertService::~PolicyCertService() {
  if (policy_certificate_provider_)
    policy_certificate_provider_->RemovePolicyProvidedCertsObserver(this);
}

PolicyCertService::PolicyCertService(
    Profile* profile,
    chromeos::PolicyCertificateProvider* policy_certificate_provider,
    bool may_use_profile_wide_trust_anchors,
    const std::string& user_id,
    user_manager::UserManager* user_manager)
    : profile_(profile),
      policy_certificate_provider_(policy_certificate_provider),
      may_use_profile_wide_trust_anchors_(may_use_profile_wide_trust_anchors),
      user_id_(user_id),
      user_manager_(user_manager) {
  DCHECK(policy_certificate_provider_);

  // Only allow using profile-wide trust anchors if a user is associated with
  // this Profile. Profiles without a user would be e.g. the sign-in screen
  // Profile which are not supposed to have profile-wide trust anchors.
  CHECK((user_manager_ && !user_id_.empty()) ||
        !may_use_profile_wide_trust_anchors_);

  policy_certificate_provider_->AddPolicyProvidedCertsObserver(this);
  profile_wide_all_server_and_authority_certs_ =
      policy_certificate_provider_->GetAllServerAndAuthorityCertificates();
  profile_wide_trust_anchors_ = GetAllowedProfileWideTrustAnchors();
}

PolicyCertService::PolicyCertService(const std::string& user_id,
                                     user_manager::UserManager* user_manager)
    : profile_(nullptr),
      policy_certificate_provider_(nullptr),
      may_use_profile_wide_trust_anchors_(true),
      user_id_(user_id),
      user_manager_(user_manager) {}

void PolicyCertService::OnPolicyProvidedCertsChanged() {
  profile_wide_all_server_and_authority_certs_ =
      policy_certificate_provider_->GetAllServerAndAuthorityCertificates();
  profile_wide_trust_anchors_ = GetAllowedProfileWideTrustAnchors();

  // Make all policy-provided server and authority certificates available to NSS
  // as temp certificates.
  // While the network service is out-of-process so it is not affected by this,
  // this is important for client certificate selection which still happens in
  // the browser process.
  // Note that this is done on the UI thread because the assumption is that NSS
  // has already been initialized by Chrome OS specific start-up code in chrome,
  // expecting that the operation of creating in-memory NSS certs is cheap in
  // that case.
  temp_policy_provided_certs_ =
      std::make_unique<network::NSSTempCertsCacheChromeOS>(
          profile_wide_all_server_and_authority_certs_);

  auto* profile_network_context =
      ProfileNetworkContextServiceFactory::GetForContext(profile_);
  profile_network_context->UpdateAdditionalCertificates();
}

void PolicyCertService::GetPolicyCertificatesForStoragePartition(
    const base::FilePath& partition_path,
    net::CertificateList* out_all_server_and_authority_certificates,
    net::CertificateList* out_trust_anchors) const {
  *out_all_server_and_authority_certificates =
      profile_wide_all_server_and_authority_certs_;
  *out_trust_anchors = profile_wide_trust_anchors_;
}

bool PolicyCertService::UsedPolicyCertificates() const {
  // PolicyCertService is only tracking if policy-provided certificates have
  // been used for profiles that are associated with a user.
  if (user_id_.empty())
    return false;

  return PolicyCertServiceFactory::UsedPolicyCertificates(user_id_);
}

net::CertificateList PolicyCertService::GetAllowedProfileWideTrustAnchors() {
  if (!may_use_profile_wide_trust_anchors_)
    return {};

  net::CertificateList trust_anchors =
      policy_certificate_provider_->GetWebTrustedCertificates();

  // Do not use certificates installed via ONC policy if the current session has
  // multiple profiles. This is important to make sure that any possibly tainted
  // data is absolutely confined to the managed profile and never, ever leaks to
  // any other profile.
  if (!trust_anchors.empty() && user_manager_ &&
      user_manager_->GetLoggedInUsers().size() > 1u) {
    LOG(ERROR) << "Ignoring ONC-pushed certificates update because multiple "
               << "users are logged in.";
    return {};
  }

  return trust_anchors;
}

// static
std::unique_ptr<PolicyCertService> PolicyCertService::CreateForTesting(
    const std::string& user_id,
    user_manager::UserManager* user_manager) {
  return base::WrapUnique(new PolicyCertService(user_id, user_manager));
}

void PolicyCertService::SetPolicyTrustAnchorsForTesting(
    const net::CertificateList& trust_anchors) {
  // Only allow this call in an instance that has been created through
  // PolicyCertService::CreateForTesting.
  CHECK_EQ(nullptr, profile_);
  CHECK_EQ(nullptr, policy_certificate_provider_);

  profile_wide_all_server_and_authority_certs_ = trust_anchors;
  profile_wide_trust_anchors_ = trust_anchors;
}

}  // namespace policy
