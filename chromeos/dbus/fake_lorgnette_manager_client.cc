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
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(scan_image_response_)));
}

void FakeLorgnetteManagerClient::StartScan(
    std::string device_name,
    const ScanProperties& properties,
    DBusMethodCallback<std::string> completion_callback,
    base::Optional<base::RepeatingCallback<void(int)>> progress_callback) {
  // Simulate progress reporting for the scan job.
  if (progress_callback.has_value()) {
    base::RepeatingCallback<void(int)> callback = progress_callback.value();
    for (int progress : {7, 22, 40, 42, 59, 74, 95}) {
      callback.Run(progress);
    }
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(completion_callback),
                                std::move(scan_image_response_)));
}

void FakeLorgnetteManagerClient::SetListScannersResponse(
    const base::Optional<lorgnette::ListScannersResponse>&
        list_scanners_response) {
  list_scanners_response_ = list_scanners_response;
}

void FakeLorgnetteManagerClient::SetScanResponse(
    const base::Optional<std::string>& scan_image_response) {
  scan_image_response_ = scan_image_response;
}

}  // namespace chromeos
