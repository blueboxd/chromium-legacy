// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_CERT_PROVISIONING_WORKER_H_
#define CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_CERT_PROVISIONING_WORKER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/chromeos/attestation/tpm_challenge_key_subtle.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_common.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "net/base/backoff_entry.h"

namespace policy {
class CloudPolicyClient;
}
class Profile;
class PrefService;

namespace chromeos {
namespace cert_provisioning {

using CertProvisioningWorkerCallback =
    base::OnceCallback<void(bool is_success)>;

class CertProvisioningWorker;

class CertProvisioningWorkerFactory {
 public:
  virtual ~CertProvisioningWorkerFactory() = default;

  static CertProvisioningWorkerFactory* Get();
  virtual std::unique_ptr<CertProvisioningWorker> Create(
      CertScope cert_scope,
      Profile* profile,
      PrefService* pref_service,
      const CertProfile& cert_profile,
      policy::CloudPolicyClient* cloud_policy_client,
      CertProvisioningWorkerCallback callback);

  // Doesn't take ownership.
  static void SetFactoryForTesting(CertProvisioningWorkerFactory* test_factory);

 private:
  static CertProvisioningWorkerFactory* test_factory_;
};

// These values are used in serialization and should be changed carefully.
enum class CertProvisioningWorkerState {
  kInitState = 0,
  kKeypairGenerated = 1,
  kStartCsrResponseReceived = 2,
  kVaChallengeFinished = 3,
  kKeyRegistered = 4,
  kSignCsrFinished = 5,
  kFinishCsrResponseReceived = 6,
  kDownloadCertResponseReceived = 7,
  kSucceed = 8,
  kFailed = 9,
  kMaxValue = kFailed,
};

class CertProvisioningWorker {
 public:
  CertProvisioningWorker() = default;
  CertProvisioningWorker(const CertProvisioningWorker&) = delete;
  CertProvisioningWorker& operator=(const CertProvisioningWorker&) = delete;
  virtual ~CertProvisioningWorker() = default;

  // Continue provisioning a certificate.
  virtual void DoStep() = 0;
  // Returns true, if the worker is waiting for some future event. |DoStep| can
  // be called to try continue right now.
  virtual bool IsWaiting() const = 0;
};

class CertProvisioningWorkerImpl : public CertProvisioningWorker {
 public:
  CertProvisioningWorkerImpl(CertScope cert_scope,
                             Profile* profile,
                             PrefService* pref_service,
                             const CertProfile& cert_profile,
                             policy::CloudPolicyClient* cloud_policy_client,
                             CertProvisioningWorkerCallback callback);
  ~CertProvisioningWorkerImpl() override;

  // CertProvisioningWorker
  void DoStep() override;
  bool IsWaiting() const override;

  // For testing.
  CertProvisioningWorkerState GetState() const;

 private:
  friend class CertProvisioningSerializer;

  void GenerateKey();
  void OnGenerateKeyDone(const attestation::TpmChallengeKeyResult& result);

  void StartCsr();
  void OnStartCsrDone(policy::DeviceManagementStatus status,
                      base::Optional<CertProvisioningResponseErrorType> error,
                      base::Optional<int64_t> try_later,
                      const std::string& invalidation_topic,
                      const std::string& va_challenge,
                      enterprise_management::HashingAlgorithm hashing_algorithm,
                      const std::string& data_to_sign);

  void BuildVaChallengeResponse();
  void OnBuildVaChallengeResponseDone(
      const attestation::TpmChallengeKeyResult& result);

  void RegisterKey();
  void OnRegisterKeyDone(const attestation::TpmChallengeKeyResult& result);

  void SignCsr();
  void OnSignCsrDone(const std::string& signature,
                     const std::string& error_message);

  void FinishCsr();
  void OnFinishCsrDone(policy::DeviceManagementStatus status,
                       base::Optional<CertProvisioningResponseErrorType> error,
                       base::Optional<int64_t> try_later);

  void DownloadCert();
  void OnDownloadCertDone(
      policy::DeviceManagementStatus status,
      base::Optional<CertProvisioningResponseErrorType> error,
      base::Optional<int64_t> try_later,
      const std::string& pem_encoded_certificate);

  void ImportCert();
  void OnImportCertDone(const std::string& error_message);

  void ScheduleNextStep(base::TimeDelta delay);
  void CancelScheduledTasks();

  // TODO: implement when invalidations are supported for cert provisioning.
  void RegisterForInvalidationTopic(const std::string& invalidation_topic) {}

  // If it is called with kSucceed or kFailed, it will call the |callback_|. The
  // worker can be destroyed in callback and should not use any member fields
  // after that.
  void UpdateState(CertProvisioningWorkerState state);
  bool IsFinished() const;

  // Returns true if there are no errors and the flow can be continued.
  bool ProcessResponseErrors(
      policy::DeviceManagementStatus status,
      base::Optional<CertProvisioningResponseErrorType> error,
      base::Optional<int64_t> try_later);

  CertScope cert_scope_ = CertScope::kUser;
  Profile* profile_ = nullptr;
  PrefService* pref_service_ = nullptr;
  CertProfile cert_profile_;
  CertProvisioningWorkerCallback callback_;

  // This field should be updated only via |UpdateState| function. It will
  // trigger update of the serialized data.
  CertProvisioningWorkerState state_ = CertProvisioningWorkerState::kInitState;
  bool is_waiting_ = false;
  // Currently it is used only for DM Server DM_STATUS_TEMPORARY_UNAVAILABLE
  // error. For all other errors the worker just gives up.
  net::BackoffEntry request_backoff_;

  std::string public_key_;
  std::string invalidation_topic_;
  std::string csr_;
  std::string va_challenge_;
  std::string va_challenge_response_;
  base::Optional<platform_keys::HashAlgorithm> hashing_algorithm_;
  std::string signature_;
  std::string pem_encoded_certificate_;

  platform_keys::PlatformKeysService* platform_keys_service_ = nullptr;
  std::unique_ptr<attestation::TpmChallengeKeySubtle>
      tpm_challenge_key_subtle_impl_;
  policy::CloudPolicyClient* cloud_policy_client_ = nullptr;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CertProvisioningWorkerImpl> weak_factory_{this};
};

}  // namespace cert_provisioning
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_CERT_PROVISIONING_WORKER_H_
