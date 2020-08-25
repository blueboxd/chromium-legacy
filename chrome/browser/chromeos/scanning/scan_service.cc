// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/scanning/scan_service.h"

#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/scanning/lorgnette_scanner_manager.h"

namespace chromeos {

namespace {

namespace mojo_ipc = scanning::mojom;

}  // namespace

ScanService::ScanService(LorgnetteScannerManager* lorgnette_scanner_manager)
    : lorgnette_scanner_manager_(lorgnette_scanner_manager) {
  DCHECK(lorgnette_scanner_manager_);
}

ScanService::~ScanService() = default;

void ScanService::GetScanners(GetScannersCallback callback) {
  lorgnette_scanner_manager_->GetScannerNames(
      base::BindOnce(&ScanService::OnScannerNamesReceived,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ScanService::BindInterface(
    mojo::PendingReceiver<mojo_ipc::ScanService> pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

void ScanService::Shutdown() {
  lorgnette_scanner_manager_ = nullptr;
  receiver_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void ScanService::OnScannerNamesReceived(
    GetScannersCallback callback,
    std::vector<std::string> scanner_names) {
  std::vector<mojo_ipc::ScannerPtr> scanners;
  scanners.reserve(scanner_names.size());
  for (const auto& name : scanner_names) {
    base::UnguessableToken id = base::UnguessableToken::Create();
    scanners.push_back(mojo_ipc::Scanner::New(id, base::UTF8ToUTF16(name)));
  }

  std::move(callback).Run(std::move(scanners));
}

}  // namespace chromeos
