// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_lorgnette_manager_client.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {

FakeLorgnetteManagerClient::FakeLorgnetteManagerClient() = default;

FakeLorgnetteManagerClient::~FakeLorgnetteManagerClient() = default;

void FakeLorgnetteManagerClient::Init(dbus::Bus* bus) {}

void FakeLorgnetteManagerClient::ListScanners(
    DBusMethodCallback<lorgnette::ListScannersResponse> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), list_scanners_response_));
}

void FakeLorgnetteManagerClient::ScanImageToString(
    std::string device_name,
    const ScanProperties& properties,
    DBusMethodCallback<std::string> callback) {
  auto it = scan_data_.find(
      std::make_tuple(device_name, properties.mode, properties.resolution_dpi));
  auto data =
      it == scan_data_.end() ? base::nullopt : base::make_optional(it->second);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(data)));
}

void FakeLorgnetteManagerClient::AddScanData(const std::string& device_name,
                                             const ScanProperties& properties,
                                             const std::string& data) {
  scan_data_[std::make_tuple(device_name, properties.mode,
                             properties.resolution_dpi)] = data;
}

void FakeLorgnetteManagerClient::SetListScannersResponse(
    const lorgnette::ListScannersResponse& list_scanners_response) {
  list_scanners_response_ = list_scanners_response;
}

}  // namespace chromeos
