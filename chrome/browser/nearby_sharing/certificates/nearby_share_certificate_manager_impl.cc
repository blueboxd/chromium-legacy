// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_manager_impl.h"

#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/proto/encrypted_metadata.pb.h"

// static
NearbyShareCertificateManagerImpl::Factory*
    NearbyShareCertificateManagerImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<NearbyShareCertificateManager>
NearbyShareCertificateManagerImpl::Factory::Create() {
  if (test_factory_) {
    return test_factory_->CreateInstance();
  }

  return base::WrapUnique(new NearbyShareCertificateManagerImpl());
}

// static
void NearbyShareCertificateManagerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

NearbyShareCertificateManagerImpl::Factory::~Factory() = default;

NearbyShareCertificateManagerImpl::NearbyShareCertificateManagerImpl() =
    default;

NearbyShareCertificateManagerImpl::~NearbyShareCertificateManagerImpl() =
    default;

NearbySharePrivateCertificate
NearbyShareCertificateManagerImpl::GetValidPrivateCertificate(
    NearbyShareVisibility visibility) {
  NOTIMPLEMENTED();
  return NearbySharePrivateCertificate(NearbyShareVisibility::kNoOne,
                                       /* not_before=*/base::Time(),
                                       nearbyshare::proto::EncryptedMetadata());
}

void NearbyShareCertificateManagerImpl::GetDecryptedPublicCertificate(
    base::span<const uint8_t> encrypted_metadata_key,
    base::span<const uint8_t> salt,
    CertDecryptedCallback callback) {
  NOTIMPLEMENTED();
}

void NearbyShareCertificateManagerImpl::DownloadPublicCertificates() {
  NOTIMPLEMENTED();
}

void NearbyShareCertificateManagerImpl::OnStart() {
  NOTIMPLEMENTED();
}

void NearbyShareCertificateManagerImpl::OnStop() {
  NOTIMPLEMENTED();
}
