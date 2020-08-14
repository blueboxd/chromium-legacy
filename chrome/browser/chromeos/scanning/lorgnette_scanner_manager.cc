// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/scanning/lorgnette_scanner_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/scanning/lorgnette_scanner_manager_util.h"
#include "chrome/browser/chromeos/scanning/zeroconf_scanner_detector.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/lorgnette/lorgnette_service.pb.h"
#include "chromeos/dbus/lorgnette_manager_client.h"
#include "chromeos/scanning/scanner.h"
#include "net/base/ip_address.h"

namespace chromeos {

namespace {

// Returns a pointer to LorgnetteManagerClient, which is used to detect and
// interact with scanners via the lorgnette D-Bus service.
LorgnetteManagerClient* GetLorgnetteManagerClient() {
  DCHECK(DBusThreadManager::IsInitialized());
  return DBusThreadManager::Get()->GetLorgnetteManagerClient();
}

// Returns the first usable device name corresponding to the highest priority
// protocol. Returns an empty string if no usable device name is found.
std::string GetUsableDeviceName(const Scanner& scanner) {
  const std::vector<ScanProtocol> prioritized_protocols{
      ScanProtocol::kEscls, ScanProtocol::kEscl, ScanProtocol::kLegacyNetwork,
      ScanProtocol::kLegacyUsb};
  for (const auto& protocol : prioritized_protocols) {
    const auto it = scanner.device_names.find(protocol);
    if (it != scanner.device_names.end()) {
      for (const ScannerDeviceName& device_name : it->second) {
        if (device_name.usable)
          return device_name.device_name;
      }
    }
  }

  return "";
}

class LorgnetteScannerManagerImpl final : public LorgnetteScannerManager {
 public:
  LorgnetteScannerManagerImpl(
      std::unique_ptr<ZeroconfScannerDetector> zeroconf_scanner_detector)
      : zeroconf_scanner_detector_(std::move(zeroconf_scanner_detector)) {
    zeroconf_scanner_detector_->RegisterScannersDetectedCallback(
        base::BindRepeating(&LorgnetteScannerManagerImpl::OnScannersDetected,
                            weak_ptr_factory_.GetWeakPtr()));
    OnScannersDetected(zeroconf_scanner_detector_->GetScanners());
  }

  ~LorgnetteScannerManagerImpl() override = default;

  // KeyedService:
  void Shutdown() override { weak_ptr_factory_.InvalidateWeakPtrs(); }

  // LorgnetteScannerManager:
  void GetScannerNames(GetScannerNamesCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    GetLorgnetteManagerClient()->ListScanners(
        base::BindOnce(&LorgnetteScannerManagerImpl::OnListScannersResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // LorgnetteScannerManager:
  void Scan(const std::string& scanner_name,
            const LorgnetteManagerClient::ScanProperties& scan_properties,
            ScanCallback callback) override {
    const auto it = deduped_scanners_.find(scanner_name);
    if (it == deduped_scanners_.end()) {
      LOG(ERROR) << "Failed to find scanner with name " << scanner_name;
      std::move(callback).Run(base::nullopt);
      return;
    }

    const std::string device_name = GetUsableDeviceName(it->second);
    if (device_name.empty()) {
      LOG(ERROR) << "Failed to find usable device name for " << scanner_name;
      std::move(callback).Run(base::nullopt);
      return;
    }

    GetLorgnetteManagerClient()->StartScan(
        device_name, scan_properties,
        base::BindOnce(
            &LorgnetteScannerManagerImpl::OnScanImageToStringResponse,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        base::nullopt);
  }

 private:
  // Called when scanners are detected by a ScannerDetector.
  void OnScannersDetected(std::vector<Scanner> scanners) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    zeroconf_scanners_ = scanners;
  }

  // Handles the result of calling LorgnetteManagerClient::ListScanners().
  void OnListScannersResponse(
      GetScannerNamesCallback callback,
      base::Optional<lorgnette::ListScannersResponse> response) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    RebuildDedupedScanners(response);
    std::vector<std::string> scanner_names;
    scanner_names.reserve(deduped_scanners_.size());
    for (const auto& entry : deduped_scanners_)
      scanner_names.push_back(entry.first);

    std::move(callback).Run(std::move(scanner_names));
  }

  // Handles the result of calling LorgnetteManagerClient::ScanImageToString().
  void OnScanImageToStringResponse(ScanCallback callback,
                                   base::Optional<std::string> scan_data) {
    std::move(callback).Run(scan_data);
  }

  // Uses |response| and zeroconf_scanners_ to rebuild deduped_scanners_.
  void RebuildDedupedScanners(
      base::Optional<lorgnette::ListScannersResponse> response) {
    ResetDedupedScanners();
    if (!response || response->scanners_size() == 0)
      return;

    // Iterate through each lorgnette scanner and add its info to an existing
    // Scanner if it has a matching IP address. Otherwise, create a new Scanner
    // for the lorgnette scanner.
    base::flat_map<net::IPAddress, Scanner*> known_ip_addresses =
        GetKnownIpAddresses();
    for (const auto& lorgnette_scanner : response->scanners()) {
      std::string ip_address_str;
      ScanProtocol protocol = ScanProtocol::kUnknown;
      ParseScannerName(lorgnette_scanner.name(), ip_address_str, protocol);
      if (!ip_address_str.empty()) {
        net::IPAddress ip_address;
        if (ip_address.AssignFromIPLiteral(ip_address_str)) {
          const auto it = known_ip_addresses.find(ip_address);
          if (it != known_ip_addresses.end()) {
            it->second->device_names[protocol].emplace(
                lorgnette_scanner.name());
            continue;
          }
        }
      }

      const bool is_usb_scanner = protocol == ScanProtocol::kLegacyUsb;
      const std::string base_name = base::StringPrintf(
          "%s %s%s", lorgnette_scanner.manufacturer().c_str(),
          lorgnette_scanner.model().c_str(), is_usb_scanner ? " (USB)" : "");
      const std::string display_name = CreateUniqueDisplayName(base_name);

      Scanner scanner;
      scanner.display_name = display_name;
      scanner.device_names[protocol].emplace(lorgnette_scanner.name());
      deduped_scanners_[display_name] = scanner;
    }
  }

  // Resets |deduped_scanners_| by clearing it and repopulating it with
  // zeroconf_scanners_.
  void ResetDedupedScanners() {
    deduped_scanners_.clear();
    deduped_scanners_.reserve(zeroconf_scanners_.size());
    for (const auto& scanner : zeroconf_scanners_)
      deduped_scanners_[scanner.display_name] = scanner;
  }

  // Returns a map of IP addresses to the scanners they correspond to in
  // deduped_scanners_. This enables deduplication of network scanners by making
  // it easy to check for and modify them using their IP addresses.
  base::flat_map<net::IPAddress, Scanner*> GetKnownIpAddresses() {
    base::flat_map<net::IPAddress, Scanner*> known_ip_addresses;
    for (auto& entry : deduped_scanners_) {
      for (const auto& ip_address : entry.second.ip_addresses)
        known_ip_addresses[ip_address] = &entry.second;
    }

    return known_ip_addresses;
  }

  // Creates a unique display name by appending a copy number to a duplicate
  // name (e.g. if Scanner Name already exists, the second instance will be
  // renamed Scanner Name (1)).
  std::string CreateUniqueDisplayName(const std::string& base_name) {
    std::string display_name = base_name;
    int i = 1;  // The first duplicate will become "Scanner Name (1)."
    while (deduped_scanners_.find(display_name) != deduped_scanners_.end()) {
      display_name = base::StringPrintf("%s (%d)", base_name.c_str(), i);
      i++;
    }

    return display_name;
  }

  // Used to detect zeroconf scanners.
  std::unique_ptr<ZeroconfScannerDetector> zeroconf_scanner_detector_;

  // The deduplicated zeroconf scanners reported by the
  // zeroconf_scanner_detector_.
  std::vector<Scanner> zeroconf_scanners_;

  // Stores the deduplicated scanners from all sources in a map of display name
  // to Scanner. Clients are given display names and can use them to interact
  // with the corresponding scanners.
  base::flat_map<std::string, Scanner> deduped_scanners_;

  SEQUENCE_CHECKER(sequence_);

  base::WeakPtrFactory<LorgnetteScannerManagerImpl> weak_ptr_factory_{this};
};

}  // namespace

// static
std::unique_ptr<LorgnetteScannerManager> LorgnetteScannerManager::Create(
    std::unique_ptr<ZeroconfScannerDetector> zeroconf_scanner_detector) {
  return std::make_unique<LorgnetteScannerManagerImpl>(
      std::move(zeroconf_scanner_detector));
}

}  // namespace chromeos
