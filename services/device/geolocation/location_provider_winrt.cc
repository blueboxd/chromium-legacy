// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/location_provider_winrt.h"

#include "base/feature_list.h"
#include "base/win/core_winrt_util.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/cpp/geolocation/geoposition.h"

namespace device {
namespace {
bool IsWinRTSupported() {
  return base::win::ResolveCoreWinRTDelayload() &&
         base::win::ScopedHString::ResolveCoreWinRTStringDelayload();
}
}  // namespace

// LocationProviderWinrt
LocationProviderWinrt::LocationProviderWinrt() = default;

LocationProviderWinrt::~LocationProviderWinrt() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  StopProvider();
}

void LocationProviderWinrt::SetUpdateCallback(
    const LocationProviderUpdateCallback& callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  location_update_callback_ = callback;
}

void LocationProviderWinrt::StartProvider(bool high_accuracy) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (permission_granted_)
    RegisterCallbacks();
}

void LocationProviderWinrt::StopProvider() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

const mojom::Geoposition& LocationProviderWinrt::GetPosition() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return last_position_;
}

void LocationProviderWinrt::OnPermissionGranted() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  permission_granted_ = true;
  if (!ValidateGeoposition(last_position_) &&
      last_position_.error_code == mojom::Geoposition::ErrorCode::NONE) {
    RegisterCallbacks();
  }
}

void LocationProviderWinrt::HandleErrorCondition(
    mojom::Geoposition::ErrorCode position_error_code,
    const std::string& position_error_message) {
  last_position_ = mojom::Geoposition();
  last_position_.error_code = position_error_code;
  last_position_.error_message = position_error_message;

  if (location_update_callback_.is_null()) {
    return;
  }

  location_update_callback_.Run(this, last_position_);
}

void LocationProviderWinrt::RegisterCallbacks() {
  if (permission_granted_) {
    HandleErrorCondition(mojom::Geoposition::ErrorCode::POSITION_UNAVAILABLE,
                         "WinRT location provider not yet implemented.");
  }
}

// static
std::unique_ptr<LocationProvider> NewSystemLocationProvider() {
  if (!base::FeatureList::IsEnabled(
          features::kWinrtGeolocationImplementation) ||
      !IsWinRTSupported()) {
    return nullptr;
  }

  return std::make_unique<LocationProviderWinrt>();
}

}  // namespace device
