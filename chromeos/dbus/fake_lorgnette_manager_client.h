// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_LORGNETTE_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_LORGNETTE_MANAGER_CLIENT_H_

#include <string>

#include "base/optional.h"
#include "chromeos/dbus/lorgnette/lorgnette_service.pb.h"
#include "chromeos/dbus/lorgnette_manager_client.h"

namespace chromeos {

class COMPONENT_EXPORT(CHROMEOS_DBUS) FakeLorgnetteManagerClient
    : public LorgnetteManagerClient {
 public:
  FakeLorgnetteManagerClient();
  FakeLorgnetteManagerClient(const FakeLorgnetteManagerClient&) = delete;
  FakeLorgnetteManagerClient& operator=(const FakeLorgnetteManagerClient&) =
      delete;
  ~FakeLorgnetteManagerClient() override;

  void Init(dbus::Bus* bus) override;

  void ListScanners(
      DBusMethodCallback<lorgnette::ListScannersResponse> callback) override;
  void ScanImageToString(std::string device_name,
                         const ScanProperties& properties,
                         DBusMethodCallback<std::string> callback) override;

  // Sets the response returned by ListScanners().
  void SetListScannersResponse(
      const base::Optional<lorgnette::ListScannersResponse>&
          list_scanners_response);

  // Sets the response returned by ScanImageToString().
  void SetScanImageToStringResponse(
      const base::Optional<std::string>& scan_image_response);

 private:
  base::Optional<lorgnette::ListScannersResponse> list_scanners_response_;
  base::Optional<std::string> scan_image_response_;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_LORGNETTE_MANAGER_CLIENT_H_
