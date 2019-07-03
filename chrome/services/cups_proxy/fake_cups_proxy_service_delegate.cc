// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_proxy/fake_cups_proxy_service_delegate.h"

namespace chromeos {
namespace printing {

std::vector<chromeos::Printer> FakeCupsProxyServiceDelegate::GetPrinters() {
  return {};
}

base::Optional<chromeos::Printer> FakeCupsProxyServiceDelegate::GetPrinter(
    const std::string& id) {
  return base::nullopt;
}

bool FakeCupsProxyServiceDelegate::IsPrinterInstalled(const Printer& printer) {
  return false;
}

scoped_refptr<base::SingleThreadTaskRunner>
FakeCupsProxyServiceDelegate::GetIOTaskRunner() {
  return nullptr;
}

void FakeCupsProxyServiceDelegate::SetupPrinter(const Printer& printer,
                                                PrinterSetupCallback cb) {}

}  // namespace printing
}  // namespace chromeos
