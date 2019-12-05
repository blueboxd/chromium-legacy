// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/external_data_handlers/device_native_printers_external_data_handler.h"

#include <utility>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/printing/bulk_printers_calculator.h"
#include "components/policy/policy_constants.h"

namespace policy {

DeviceNativePrintersExternalDataHandler::
    DeviceNativePrintersExternalDataHandler(
        PolicyService* policy_service,
        base::WeakPtr<chromeos::BulkPrintersCalculator> calculator)
    : calculator_(calculator),
      device_native_printers_observer_(
          std::make_unique<DeviceCloudExternalDataPolicyObserver>(
              policy_service,
              key::kDeviceNativePrinters,
              this)) {}

DeviceNativePrintersExternalDataHandler::
    ~DeviceNativePrintersExternalDataHandler() = default;

void DeviceNativePrintersExternalDataHandler::OnDeviceExternalDataSet(
    const std::string& policy) {
  if (calculator_)
    calculator_->ClearData();
}

void DeviceNativePrintersExternalDataHandler::OnDeviceExternalDataCleared(
    const std::string& policy) {
  if (calculator_)
    calculator_->ClearData();
}

void DeviceNativePrintersExternalDataHandler::OnDeviceExternalDataFetched(
    const std::string& policy,
    std::unique_ptr<std::string> data,
    const base::FilePath& file_path) {
  if (calculator_)
    calculator_->SetData(std::move(data));
}

void DeviceNativePrintersExternalDataHandler::Shutdown() {
  device_native_printers_observer_.reset();
}

}  // namespace policy
