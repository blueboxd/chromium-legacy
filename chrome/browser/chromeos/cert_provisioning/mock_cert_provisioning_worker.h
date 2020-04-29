// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_MOCK_CERT_PROVISIONING_WORKER_H_
#define CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_MOCK_CERT_PROVISIONING_WORKER_H_

#include "base/containers/queue.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_worker.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {
namespace cert_provisioning {

class CertProvisioningWorkerFactoryForTesting
    : public CertProvisioningWorkerFactory {
 public:
  CertProvisioningWorkerFactoryForTesting();
  ~CertProvisioningWorkerFactoryForTesting() override;

  std::unique_ptr<CertProvisioningWorker> Create(
      CertScope cert_scope,
      Profile* profile,
      PrefService* pref_service,
      const CertProfile& cert_profile,
      policy::CloudPolicyClient* cloud_policy_client,
      CertProvisioningWorkerCallback callback) override;

  void Push(std::unique_ptr<CertProvisioningWorker> worker);
  size_t ResultsCount() const;
  void Reset();

 private:
  base::queue<std::unique_ptr<CertProvisioningWorker>> results_queue_;
};

class MockCertProvisioningWorker : public CertProvisioningWorker {
 public:
  MockCertProvisioningWorker();
  ~MockCertProvisioningWorker() override;

  MOCK_METHOD(void, DoStep, (), (override));
  MOCK_METHOD(bool, IsWaiting, (), (const override));

  void SetExpectations(testing::Cardinality do_step_times, bool is_waiting);
};

}  // namespace cert_provisioning
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_MOCK_CERT_PROVISIONING_WORKER_H_
