// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_LORGNETTE_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_LORGNETTE_MANAGER_CLIENT_H_

#include <map>
#include <string>
#include <tuple>

#include "chromeos/dbus/lorgnette/lorgnette_service.pb.h"
#include "chromeos/dbus/lorgnette_manager_client.h"

namespace chromeos {

// Lorgnette LorgnetteManagerClient implementation used on Linux desktop,
// which does nothing.
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

  // Adds a fake scan data, which will be returned by ScanImageToString(),
  // if |device_name| and |properties| are matched.
  void AddScanData(const std::string& device_name,
                   const ScanProperties& properties,
                   const std::string& data);

  // Sets the response returned by ListScanners().
  void SetListScannersResponse(
      const lorgnette::ListScannersResponse& list_scanners_response);

 private:
  lorgnette::ListScannersResponse list_scanners_response_;

  // Use tuple for a map below, which has pre-defined "less", for convenience.
  using ScanDataKey = std::tuple<std::string /* device_name */,
                                 std::string /* ScanProperties.mode */,
                                 int /* Scanproperties.resolution_dpi */>;
  std::map<ScanDataKey, std::string /* data */> scan_data_;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_LORGNETTE_MANAGER_CLIENT_H_
