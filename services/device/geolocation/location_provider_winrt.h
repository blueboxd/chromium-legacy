// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_LOCATION_PROVIDER_WINRT_H_
#define SERVICES_DEVICE_GEOLOCATION_LOCATION_PROVIDER_WINRT_H_

#include "base/threading/thread_checker.h"
#include "services/device/public/cpp/geolocation/location_provider.h"
#include "services/device/public/mojom/geoposition.mojom.h"

namespace device {

// Location provider for Windows 8/10 using the WinRT platform apis
class LocationProviderWinrt : public LocationProvider {
 public:
  LocationProviderWinrt();
  ~LocationProviderWinrt() override;

  // LocationProvider implementation.
  void SetUpdateCallback(
      const LocationProviderUpdateCallback& callback) override;
  void StartProvider(bool high_accuracy) override;
  void StopProvider() override;
  const mojom::Geoposition& GetPosition() override;
  void OnPermissionGranted() override;

 private:
  void HandleErrorCondition(mojom::Geoposition::ErrorCode position_error_code,
                            const std::string& position_error_message);
  void RegisterCallbacks();

  mojom::Geoposition last_position_;
  LocationProviderUpdateCallback location_update_callback_;
  bool permission_granted_ = false;
  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(LocationProviderWinrt);
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_LOCATION_PROVIDER_WINRT_H_
