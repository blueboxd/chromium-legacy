// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/attestation/fake_attestation_client.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "third_party/cros_system_api/dbus/attestation/dbus-constants.h"

namespace chromeos {
namespace {

// Posts |callback| on the current thread's task runner, passing it the
// |response| message.
template <class ReplyType>
void PostProtoResponse(base::OnceCallback<void(const ReplyType&)> callback,
                       const ReplyType& reply) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), reply));
}

}  // namespace

FakeAttestationClient::FakeAttestationClient() = default;

FakeAttestationClient::~FakeAttestationClient() = default;

void FakeAttestationClient::GetKeyInfo(
    const ::attestation::GetKeyInfoRequest& request,
    GetKeyInfoCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::GetEndorsementInfo(
    const ::attestation::GetEndorsementInfoRequest& request,
    GetEndorsementInfoCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::GetAttestationKeyInfo(
    const ::attestation::GetAttestationKeyInfoRequest& request,
    GetAttestationKeyInfoCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::ActivateAttestationKey(
    const ::attestation::ActivateAttestationKeyRequest& request,
    ActivateAttestationKeyCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::CreateCertifiableKey(
    const ::attestation::CreateCertifiableKeyRequest& request,
    CreateCertifiableKeyCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::Decrypt(
    const ::attestation::DecryptRequest& request,
    DecryptCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::Sign(const ::attestation::SignRequest& request,
                                 SignCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::RegisterKeyWithChapsToken(
    const ::attestation::RegisterKeyWithChapsTokenRequest& request,
    RegisterKeyWithChapsTokenCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::GetEnrollmentPreparations(
    const ::attestation::GetEnrollmentPreparationsRequest& request,
    GetEnrollmentPreparationsCallback callback) {
  bool is_prepared = is_prepared_;
  // Override the state if there is a customized sequence.
  if (!preparation_sequences_.empty()) {
    is_prepared = preparation_sequences_.front();
    preparation_sequences_.pop_front();
  }

  ::attestation::GetEnrollmentPreparationsReply reply;
  if (is_prepared) {
    (*reply.mutable_enrollment_preparations())[request.aca_type()] = true;
  }
  PostProtoResponse(std::move(callback), reply);
}

void FakeAttestationClient::GetStatus(
    const ::attestation::GetStatusRequest& request,
    GetStatusCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::Verify(const ::attestation::VerifyRequest& request,
                                   VerifyCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::CreateEnrollRequest(
    const ::attestation::CreateEnrollRequestRequest& request,
    CreateEnrollRequestCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::FinishEnroll(
    const ::attestation::FinishEnrollRequest& request,
    FinishEnrollCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::CreateCertificateRequest(
    const ::attestation::CreateCertificateRequestRequest& request,
    CreateCertificateRequestCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::FinishCertificateRequest(
    const ::attestation::FinishCertificateRequestRequest& request,
    FinishCertificateRequestCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::Enroll(const ::attestation::EnrollRequest& request,
                                   EnrollCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::GetCertificate(
    const ::attestation::GetCertificateRequest& request,
    GetCertificateCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::SignEnterpriseChallenge(
    const ::attestation::SignEnterpriseChallengeRequest& request,
    SignEnterpriseChallengeCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::SignSimpleChallenge(
    const ::attestation::SignSimpleChallengeRequest& request,
    SignSimpleChallengeCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::SetKeyPayload(
    const ::attestation::SetKeyPayloadRequest& request,
    SetKeyPayloadCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::DeleteKeys(
    const ::attestation::DeleteKeysRequest& request,
    DeleteKeysCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::ResetIdentity(
    const ::attestation::ResetIdentityRequest& request,
    ResetIdentityCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::GetEnrollmentId(
    const ::attestation::GetEnrollmentIdRequest& request,
    GetEnrollmentIdCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::GetCertifiedNvIndex(
    const ::attestation::GetCertifiedNvIndexRequest& request,
    GetCertifiedNvIndexCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::ConfigureEnrollmentPreparations(bool is_prepared) {
  is_prepared_ = is_prepared;
}

void FakeAttestationClient::ConfigureEnrollmentPreparationsSequence(
    std::deque<bool> sequence) {
  preparation_sequences_ = std::move(sequence);
}

AttestationClient::TestInterface* FakeAttestationClient::GetTestInterface() {
  return this;
}

}  // namespace chromeos
