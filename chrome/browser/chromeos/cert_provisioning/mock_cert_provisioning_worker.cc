// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cert_provisioning/mock_cert_provisioning_worker.h"

using testing::Return;

namespace chromeos {
namespace cert_provisioning {

// ================ CertProvisioningWorkerFactoryForTesting ====================

CertProvisioningWorkerFactoryForTesting::
    CertProvisioningWorkerFactoryForTesting() = default;

CertProvisioningWorkerFactoryForTesting::
    ~CertProvisioningWorkerFactoryForTesting() = default;

std::unique_ptr<CertProvisioningWorker>
CertProvisioningWorkerFactoryForTesting::Create(
    CertScope cert_scope,
    Profile* profile,
    PrefService* pref_service,
    const CertProfile& cert_profile,
    policy::CloudPolicyClient* cloud_policy_client,
    CertProvisioningWorkerCallback callback) {
  CHECK(!results_queue_.empty());

  std::unique_ptr<CertProvisioningWorker> result =
      std::move(results_queue_.front());
  results_queue_.pop();
  return result;
}

void CertProvisioningWorkerFactoryForTesting::Push(
    std::unique_ptr<CertProvisioningWorker> worker) {
  results_queue_.push(std::move(worker));
}

size_t CertProvisioningWorkerFactoryForTesting::ResultsCount() const {
  return results_queue_.size();
}

void CertProvisioningWorkerFactoryForTesting::Reset() {
  results_queue_ = {};
}

// ================ MockCertProvisioningWorker =================================

MockCertProvisioningWorker::MockCertProvisioningWorker() = default;
MockCertProvisioningWorker::~MockCertProvisioningWorker() = default;

void MockCertProvisioningWorker::SetExpectations(
    testing::Cardinality do_step_times,
    bool is_waiting) {
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, DoStep).Times(do_step_times);
  EXPECT_CALL(*this, IsWaiting).WillRepeatedly(Return(is_waiting));
}

}  // namespace cert_provisioning
}  // namespace chromeos
