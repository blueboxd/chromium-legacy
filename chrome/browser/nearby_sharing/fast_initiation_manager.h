// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_FAST_INITIATION_MANAGER_H_
#define CHROME_BROWSER_NEARBY_SHARING_FAST_INITIATION_MANAGER_H_

#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_adapter.h"

// FastInitiationManager broadcasts advertisements with the service UUID
// 0xFE2C. The broadcast data will be
// 0xFC128E along with 2 additional bytes of metadata at the end. Some remote
// devices background scan for Fast Initiation advertisements, as a signal to
// begin advertising via Nearby Connections.
class FastInitiationManager : device::BluetoothAdvertisement::Observer {
 public:
  explicit FastInitiationManager(
      scoped_refptr<device::BluetoothAdapter> adapter);
  ~FastInitiationManager() override;
  FastInitiationManager(const FastInitiationManager&) = delete;
  FastInitiationManager& operator=(const FastInitiationManager&) = delete;

  // Begin broadcasting Fast Initiation advertisement.
  void StartAdvertising();

  // Stop broadcasting Fast Initiation advertisement.
  void StopAdvertising();

 private:
  void OnAdvertisementRegistered(
      scoped_refptr<device::BluetoothAdvertisement> advertisement);
  void OnErrorRegisteringAdvertisement(
      device::BluetoothAdvertisement::ErrorCode error_code);
  uint8_t GenerateFastInitV1Metadata();
  void AdvertisementReleased(
      device::BluetoothAdvertisement* advertisement) override;

  scoped_refptr<device::BluetoothAdapter> adapter_;
  scoped_refptr<device::BluetoothAdvertisement> advertisement_;
  base::WeakPtrFactory<FastInitiationManager> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_FAST_INITIATION_MANAGER_H_
