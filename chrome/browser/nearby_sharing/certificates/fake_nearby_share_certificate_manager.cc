// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/nearby_sharing/certificates/fake_nearby_share_certificate_manager.h"
#include "chrome/browser/nearby_sharing/certificates/test_util.h"

FakeNearbyShareCertificateManager::Factory::Factory() = default;

FakeNearbyShareCertificateManager::Factory::~Factory() = default;

std::unique_ptr<NearbyShareCertificateManager>
FakeNearbyShareCertificateManager::Factory::CreateInstance() {
  auto instance = std::make_unique<FakeNearbyShareCertificateManager>();
  instances_.push_back(instance.get());

  return instance;
}

FakeNearbyShareCertificateManager::GetDecryptedPublicCertificateCall::
    GetDecryptedPublicCertificateCall(
        base::span<const uint8_t> encrypted_metadata_key,
        base::span<const uint8_t> salt,
        CertDecryptedCallback callback)
    : encrypted_metadata_key(encrypted_metadata_key.begin(),
                             encrypted_metadata_key.end()),
      salt(salt.begin(), salt.end()),
      callback(std::move(callback)) {}

FakeNearbyShareCertificateManager::GetDecryptedPublicCertificateCall::
    GetDecryptedPublicCertificateCall(
        GetDecryptedPublicCertificateCall&& other) = default;

FakeNearbyShareCertificateManager::GetDecryptedPublicCertificateCall&
FakeNearbyShareCertificateManager::GetDecryptedPublicCertificateCall::operator=(
    GetDecryptedPublicCertificateCall&& other) = default;

FakeNearbyShareCertificateManager::GetDecryptedPublicCertificateCall::
    ~GetDecryptedPublicCertificateCall() = default;

FakeNearbyShareCertificateManager::FakeNearbyShareCertificateManager() =
    default;

FakeNearbyShareCertificateManager::~FakeNearbyShareCertificateManager() =
    default;

NearbySharePrivateCertificate
FakeNearbyShareCertificateManager::GetValidPrivateCertificate(
    NearbyShareVisibility visibility) {
  ++num_get_valid_private_certificate_calls_;
  return GetNearbyShareTestPrivateCertificate(visibility);
}

void FakeNearbyShareCertificateManager::GetDecryptedPublicCertificate(
    base::span<const uint8_t> encrypted_metadata_key,
    base::span<const uint8_t> salt,
    CertDecryptedCallback callback) {
  get_decrypted_public_certificate_calls_.emplace_back(
      encrypted_metadata_key, salt, std::move(callback));
}

void FakeNearbyShareCertificateManager::DownloadPublicCertificates() {
  ++num_download_public_certificates_calls_;
}

void FakeNearbyShareCertificateManager::OnStart() {}

void FakeNearbyShareCertificateManager::OnStop() {}
