// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SCANNING_LORGNETTE_SCANNER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_SCANNING_LORGNETTE_SCANNER_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/optional.h"
#include "chromeos/dbus/lorgnette_manager_client.h"
#include "components/keyed_service/core/keyed_service.h"

namespace chromeos {

class ZeroconfScannerDetector;

// Top-level manager of available scanners in Chrome OS. All functions in this
// class must be called from a sequenced context.
class LorgnetteScannerManager : public KeyedService {
 public:
  using GetScannerNamesCallback =
      base::OnceCallback<void(std::vector<std::string> scanner_names)>;
  using ScanCallback =
      base::OnceCallback<void(base::Optional<std::string> scan_data)>;

  ~LorgnetteScannerManager() override = default;

  static std::unique_ptr<LorgnetteScannerManager> Create(
      std::unique_ptr<ZeroconfScannerDetector> zeroconf_scanner_detector);

  // Returns the names of all available, deduplicated scanners.
  virtual void GetScannerNames(GetScannerNamesCallback callback) = 0;

  // Performs a scan with the scanner specified by |scanner_name| using the
  // given |scan_properties|. If |scanner_name| does not correspond to a known
  // scanner, base::nullopt is returned in the callback.
  virtual void Scan(
      const std::string& scanner_name,
      const LorgnetteManagerClient::ScanProperties& scan_properties,
      ScanCallback callback) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SCANNING_LORGNETTE_SCANNER_MANAGER_H_
