// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/fast_initiation_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_advertisement.h"

namespace {
constexpr const char kNearbySharingFastInitiationServiceUuid[] =
    "0000fe2c-0000-1000-8000-00805f9b34fb";
const uint8_t kNearbySharingFastPairId[] = {0xfc, 0x12, 0x8e};
}  // namespace

FastInitiationManager::FastInitiationManager(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  DCHECK(adapter && adapter->IsPresent() && adapter->IsPowered());
  adapter_ = adapter;
}

FastInitiationManager::~FastInitiationManager() {
  StopAdvertising();
}

void FastInitiationManager::StartAdvertising() {
  DCHECK(adapter_->IsPresent() && adapter_->IsPowered());

  // TODO(hansenmichael): Lower Bluetooth advertising interval to 100ms for
  // faster discovery. Be sure to restore the interval when we stop
  // broadcasting.

  auto advertisement_data =
      std::make_unique<device::BluetoothAdvertisement::Data>(
          device::BluetoothAdvertisement::ADVERTISEMENT_TYPE_BROADCAST);

  auto list = std::make_unique<device::BluetoothAdvertisement::UUIDList>();
  list->push_back(kNearbySharingFastInitiationServiceUuid);
  advertisement_data->set_service_uuids(std::move(list));

  auto service_data =
      std::make_unique<device::BluetoothAdvertisement::ServiceData>();
  auto payload = std::vector<uint8_t>(std::begin(kNearbySharingFastPairId),
                                      std::end(kNearbySharingFastPairId));
  payload.push_back(GenerateFastInitV1Metadata());
  service_data->insert(std::pair<std::string, std::vector<uint8_t>>(
      kNearbySharingFastInitiationServiceUuid, payload));
  advertisement_data->set_service_data(std::move(service_data));

  adapter_->RegisterAdvertisement(
      std::move(advertisement_data),
      base::Bind(&FastInitiationManager::OnAdvertisementRegistered,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&FastInitiationManager::OnErrorRegisteringAdvertisement,
                 weak_ptr_factory_.GetWeakPtr()));
}

void FastInitiationManager::StopAdvertising() {
  if (advertisement_) {
    // TODO(hansenmichael): Implement real callback and errback.
    advertisement_->RemoveObserver(this);
    advertisement_->Unregister(base::DoNothing(), base::DoNothing());
    advertisement_.reset();
  }
}

void FastInitiationManager::OnAdvertisementRegistered(
    scoped_refptr<device::BluetoothAdvertisement> advertisement) {
  advertisement_ = advertisement;

  // TODO(hansenmichael): Implement advertisement observer methods.
  advertisement_->AddObserver(this);

  // TODO(hansenmichael): Inform the StartAdvertising() caller of success.
}

void FastInitiationManager::OnErrorRegisteringAdvertisement(
    device::BluetoothAdvertisement::ErrorCode error_code) {
  // TODO(hansenmichael): Inform the StartAdvertising() caller of failure.
}

uint8_t FastInitiationManager::GenerateFastInitV1Metadata() {
  // TODO(hansenmichael): Include 'version', |type|, and |adjusted_tx_power|
  // bits.
  return 0x00;
}

void FastInitiationManager::AdvertisementReleased(
    device::BluetoothAdvertisement* advertisement) {
  // TODO(hansenmichael): Handle advertisement released appropriately.
}
