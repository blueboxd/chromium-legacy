// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_FAST_INITIATION_MANAGER_H_
#define CHROME_BROWSER_NEARBY_SHARING_FAST_INITIATION_MANAGER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_adapter.h"

// FastInitiationManager broadcasts advertisements with the service UUID
// 0xFE2C. The broadcast data will be
// 0xFC128E along with 2 additional bytes of metadata at the end. Some remote
// devices background scan for Fast Initiation advertisements, as a signal to
// begin advertising via Nearby Connections.
class FastInitiationManager : device::BluetoothAdvertisement::Observer {
 public:
  class Factory {
   public:
    Factory() = default;
    Factory(const Factory&) = delete;
    Factory& operator=(const Factory&) = delete;

    static std::unique_ptr<FastInitiationManager> Create(
        scoped_refptr<device::BluetoothAdapter> adapter);

    static void SetFactoryForTesting(Factory* factory);

   protected:
    virtual ~Factory() = default;
    virtual std::unique_ptr<FastInitiationManager> CreateInstance(
        scoped_refptr<device::BluetoothAdapter> adapter) = 0;

   private:
    static Factory* factory_instance_;
  };

  explicit FastInitiationManager(
      scoped_refptr<device::BluetoothAdapter> adapter);
  ~FastInitiationManager() override;
  FastInitiationManager(const FastInitiationManager&) = delete;
  FastInitiationManager& operator=(const FastInitiationManager&) = delete;

  // Begin broadcasting Fast Initiation advertisement.
  virtual void StartAdvertising(base::OnceCallback<void()> callback,
                                base::OnceCallback<void()> error_callback);

  // Stop broadcasting Fast Initiation advertisement.
  virtual void StopAdvertising(base::OnceCallback<void()> callback);

 private:
  // device::BluetoothAdvertisement::Observer:
  void AdvertisementReleased(
      device::BluetoothAdvertisement* advertisement) override;

  void OnAdvertisementRegistered(
      scoped_refptr<device::BluetoothAdvertisement> advertisement);
  void OnErrorRegisteringAdvertisement(
      device::BluetoothAdvertisement::ErrorCode error_code);
  void OnAdvertisementUnregistered();
  void OnErrorUnregisteringAdvertisement(
      device::BluetoothAdvertisement::ErrorCode error_code);
  uint8_t GenerateFastInitV1Metadata();

  scoped_refptr<device::BluetoothAdapter> adapter_;
  scoped_refptr<device::BluetoothAdvertisement> advertisement_;
  base::OnceCallback<void()> start_callback_;
  base::OnceCallback<void()> start_error_callback_;
  base::OnceCallback<void()> stop_callback_;
  base::WeakPtrFactory<FastInitiationManager> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_FAST_INITIATION_MANAGER_H_
