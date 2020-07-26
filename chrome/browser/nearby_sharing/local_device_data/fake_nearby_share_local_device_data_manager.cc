// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/local_device_data/fake_nearby_share_local_device_data_manager.h"

#include <utility>

namespace {
const char kDefaultId[] = "123456789A";
}  // namespace

FakeNearbyShareLocalDeviceDataManager::Factory::Factory() = default;

FakeNearbyShareLocalDeviceDataManager::Factory::~Factory() = default;

std::unique_ptr<NearbyShareLocalDeviceDataManager>
FakeNearbyShareLocalDeviceDataManager::Factory::CreateInstance(
    PrefService* pref_service,
    NearbyShareClientFactory* http_client_factory) {
  latest_pref_service_ = pref_service;
  latest_http_client_factory_ = http_client_factory;

  auto instance = std::make_unique<FakeNearbyShareLocalDeviceDataManager>();
  instances_.push_back(instance.get());

  return instance;
}

FakeNearbyShareLocalDeviceDataManager::UploadContactsCall::UploadContactsCall(
    std::vector<nearbyshare::proto::Contact> contacts,
    UploadCompleteCallback callback)
    : contacts(std::move(contacts)), callback(std::move(callback)) {}

FakeNearbyShareLocalDeviceDataManager::UploadContactsCall::UploadContactsCall(
    UploadContactsCall&&) = default;

FakeNearbyShareLocalDeviceDataManager::UploadContactsCall::
    ~UploadContactsCall() = default;

FakeNearbyShareLocalDeviceDataManager::UploadCertificatesCall::
    UploadCertificatesCall(
        std::vector<nearbyshare::proto::PublicCertificate> certificates,
        UploadCompleteCallback callback)
    : certificates(std::move(certificates)), callback(std::move(callback)) {}

FakeNearbyShareLocalDeviceDataManager::UploadCertificatesCall::
    UploadCertificatesCall(UploadCertificatesCall&&) = default;

FakeNearbyShareLocalDeviceDataManager::UploadCertificatesCall::
    ~UploadCertificatesCall() = default;

FakeNearbyShareLocalDeviceDataManager::FakeNearbyShareLocalDeviceDataManager()
    : id_(kDefaultId) {}

FakeNearbyShareLocalDeviceDataManager::
    ~FakeNearbyShareLocalDeviceDataManager() = default;

std::string FakeNearbyShareLocalDeviceDataManager::GetId() {
  return id_;
}

base::Optional<std::string>
FakeNearbyShareLocalDeviceDataManager::GetDeviceName() const {
  return device_name_;
}

base::Optional<std::string> FakeNearbyShareLocalDeviceDataManager::GetFullName()
    const {
  return full_name_;
}

base::Optional<std::string> FakeNearbyShareLocalDeviceDataManager::GetIconUrl()
    const {
  return icon_url_;
}

void FakeNearbyShareLocalDeviceDataManager::SetDeviceName(
    const std::string& name) {
  device_name_ = name;
}

void FakeNearbyShareLocalDeviceDataManager::DownloadDeviceData() {
  ++num_download_device_data_calls_;
}

void FakeNearbyShareLocalDeviceDataManager::UploadContacts(
    std::vector<nearbyshare::proto::Contact> contacts,
    UploadCompleteCallback callback) {
  upload_contacts_calls_.emplace_back(std::move(contacts), std::move(callback));
}

void FakeNearbyShareLocalDeviceDataManager::UploadCertificates(
    std::vector<nearbyshare::proto::PublicCertificate> certificates,
    UploadCompleteCallback callback) {
  upload_certificates_calls_.emplace_back(std::move(certificates),
                                          std::move(callback));
}

void FakeNearbyShareLocalDeviceDataManager::OnStart() {}

void FakeNearbyShareLocalDeviceDataManager::OnStop() {}
