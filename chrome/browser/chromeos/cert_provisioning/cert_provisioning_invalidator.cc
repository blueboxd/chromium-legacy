// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_invalidator.h"

#include <utility>

#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_common.h"
#include "components/invalidation/public/invalidator_state.h"
#include "components/invalidation/public/topic_invalidation_map.h"

namespace chromeos {
namespace cert_provisioning {

namespace {

// Shall be expanded to cert.[scope].[topic]
const char* kOwnerNameFormat = "cert.%s.%s";

const char* CertScopeToString(CertScope scope) {
  switch (scope) {
    case CertScope::kUser:
      return "user";
    case CertScope::kDevice:
      return "device";
  }

  NOTREACHED() << "Unknown cert scope: " << static_cast<int>(scope);
  return "";
}

}  // namespace

// static
std::unique_ptr<CertProvisioningInvalidator>
CertProvisioningInvalidator::BuildAndRegister(
    CertScope scope,
    invalidation::InvalidationService* invalidation_service,
    const syncer::Topic& topic,
    CertProvisioningInvalidator::OnInvalidationCallback
        on_invalidation_callback) {
  auto invalidator = std::make_unique<CertProvisioningInvalidator>(
      scope, invalidation_service, topic, std::move(on_invalidation_callback));

  if (!invalidator->Register()) {
    return nullptr;
  }

  return invalidator;
}

CertProvisioningInvalidator::CertProvisioningInvalidator(
    CertScope scope,
    invalidation::InvalidationService* invalidation_service,
    const syncer::Topic& topic,
    OnInvalidationCallback on_invalidation_callback)
    : scope_(scope),
      invalidation_service_(invalidation_service),
      topic_(topic),
      on_invalidation_callback_(std::move(on_invalidation_callback)) {
  DCHECK(invalidation_service_);
  DCHECK(!on_invalidation_callback_.is_null());
}

CertProvisioningInvalidator::~CertProvisioningInvalidator() {
  // Unregister is not called here so that subscription can be preserved if
  // browser restarts. If subscription is not needed a user must call Unregister
  // explicitly.
}

bool CertProvisioningInvalidator::Register() {
  if (state_.is_registered) {
    return true;
  }

  OnInvalidatorStateChange(invalidation_service_->GetInvalidatorState());

  invalidation_service_observer_.Add(invalidation_service_);

  if (!invalidation_service_->UpdateInterestedTopics(this,
                                                     /*topics=*/{topic_})) {
    LOG(WARNING) << "Failed to register with topic " << topic_;
    return false;
  }

  state_.is_registered = true;
  return true;
}

void CertProvisioningInvalidator::Unregister() {
  if (!state_.is_registered) {
    return;
  }

  // Assuming that updating invalidator's topics with empty set can never fail
  // since failure is only possible non-empty set with topic associated with
  // some other invalidator.
  const bool topics_reset =
      invalidation_service_->UpdateInterestedTopics(this, syncer::TopicSet());
  DCHECK(topics_reset);
  invalidation_service_observer_.Remove(invalidation_service_);

  state_.is_registered = false;
}

void CertProvisioningInvalidator::OnInvalidatorStateChange(
    syncer::InvalidatorState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  state_.is_invalidation_service_enabled =
      state == syncer::INVALIDATIONS_ENABLED;
}

void CertProvisioningInvalidator::OnIncomingInvalidation(
    const syncer::TopicInvalidationMap& invalidation_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!state_.is_invalidation_service_enabled) {
    LOG(WARNING) << "Unexpected invalidation received.";
  }

  const syncer::SingleObjectInvalidationSet& list =
      invalidation_map.ForTopic(topic_);
  if (list.IsEmpty()) {
    NOTREACHED() << "Incoming invlaidation does not contain invalidation"
                    " for certificate topic";
    return;
  }

  for (const auto& it : list) {
    it.Acknowledge();
  }

  on_invalidation_callback_.Run();
}

std::string CertProvisioningInvalidator::GetOwnerName() const {
  return base::StringPrintf(kOwnerNameFormat, CertScopeToString(scope_),
                            topic_.c_str());
}

bool CertProvisioningInvalidator::IsPublicTopic(
    const syncer::Topic& /*topic*/) const {
  return false;
}

}  // namespace cert_provisioning
}  // namespace chromeos

