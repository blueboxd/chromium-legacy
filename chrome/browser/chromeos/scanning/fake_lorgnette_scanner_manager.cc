// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/scanning/fake_lorgnette_scanner_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {

FakeLorgnetteScannerManager::FakeLorgnetteScannerManager() = default;

FakeLorgnetteScannerManager::~FakeLorgnetteScannerManager() = default;

void FakeLorgnetteScannerManager::GetScannerNames(
    GetScannerNamesCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), scanner_names_));
}

void FakeLorgnetteScannerManager::Scan(
    const std::string& scanner_name,
    const LorgnetteManagerClient::ScanProperties& scan_properties,
    ScanCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(scan_data_)));
}

void FakeLorgnetteScannerManager::SetGetScannerNamesResponse(
    const std::vector<std::string>& scanner_names) {
  scanner_names_ = scanner_names;
}

void FakeLorgnetteScannerManager::SetScanResponse(
    const base::Optional<std::string>& scan_data) {
  scan_data_ = scan_data;
}

}  // namespace chromeos
