// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/scanning/lorgnette_scanner_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "chrome/browser/chromeos/scanning/zeroconf_scanner_detector.h"
#include "chrome/browser/chromeos/scanning/zeroconf_scanner_detector_utils.h"
#include "chrome/browser/local_discovery/service_discovery_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_lorgnette_manager_client.h"
#include "chromeos/dbus/lorgnette/lorgnette_service.pb.h"
#include "chromeos/scanning/scanner.h"
#include "net/base/ip_address.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

using local_discovery::ServiceDescription;
using ::testing::ElementsAreArray;

// Test device names for different types of lorgnette scanners.
constexpr char kLorgnetteNetworkIpDeviceName[] = "test:MX3100_192.168.0.3";
constexpr char kLorgnetteNetworkUrlDeviceName[] =
    "http://testscanner.domain.org";
constexpr char kLorgnetteUsbDeviceName[] = "test:04A91752_94370B";

// A scanner name that does not correspond to a known scanner.
constexpr char kUnknownScannerName[] = "Unknown Scanner";

// Returns a ScannerInfo object with the given |name|.
lorgnette::ScannerInfo CreateLorgnetteScanner(std::string name) {
  lorgnette::ScannerInfo scanner;
  scanner.set_name(name);
  scanner.set_manufacturer("Test");
  scanner.set_model("MX3100");
  scanner.set_type("Flatbed");
  return scanner;
}

// Returns a ListScannersResponse containing a single ScannerInfo object created
// with the given |name|.
lorgnette::ListScannersResponse CreateListScannersResponse(std::string name) {
  lorgnette::ScannerInfo scanner = CreateLorgnetteScanner(name);
  lorgnette::ListScannersResponse response;
  *response.add_scanners() = std::move(scanner);
  return response;
}

// Returns a zeroconf Scanner with the device name marked as |usable|.
Scanner CreateZeroconfScanner(bool usable = true) {
  return CreateSaneAirscanScanner("Test MX3100",
                                  ZeroconfScannerDetector::kEsclsServiceType,
                                  "", net::IPAddress(192, 168, 0, 3), 5, usable)
      .value();
}

class FakeZeroconfScannerDetector final : public ZeroconfScannerDetector {
 public:
  FakeZeroconfScannerDetector() = default;
  ~FakeZeroconfScannerDetector() override = default;

  void RegisterScannersDetectedCallback(
      OnScannersDetectedCallback callback) override {
    on_scanners_detected_callback_ = std::move(callback);
  }

  std::vector<Scanner> GetScanners() override {
    std::vector<Scanner> scanners;
    for (const auto& entry : scanners_)
      scanners.push_back(entry.second);

    return scanners;
  }

  void OnDeviceChanged(const std::string& service_type,
                       bool added,
                       const ServiceDescription& service_description) override {
  }

  void OnDeviceRemoved(const std::string& service_type,
                       const std::string& service_name) override {}

  void OnDeviceCacheFlushed(const std::string& service_type) override {}

  // Used to trigger on_scanners_detected_callback_ after adding the given
  // |scanners| to the detected scanners.
  void AddDetections(const std::vector<Scanner>& scanners) {
    for (const auto& scanner : scanners)
      scanners_[scanner.display_name] = scanner;

    on_scanners_detected_callback_.Run(GetScanners());
  }

  // Used to trigger on_scanners_detected_callback_ after removing the given
  // |scanners| from the detected scanners.
  void RemoveDetections(const std::vector<Scanner>& scanners) {
    for (const auto& scanner : scanners)
      scanners_.erase(scanner.display_name);

    on_scanners_detected_callback_.Run(GetScanners());
  }

 private:
  base::flat_map<std::string, Scanner> scanners_;
  OnScannersDetectedCallback on_scanners_detected_callback_;
};

}  // namespace

class LorgnetteScannerManagerTest : public testing::Test {
 public:
  LorgnetteScannerManagerTest() {
    run_loop_ = std::make_unique<base::RunLoop>();
    DBusThreadManager::Initialize();
    auto fake_zeroconf_scanner_detector =
        std::make_unique<FakeZeroconfScannerDetector>();
    fake_zeroconf_scanner_detector_ = fake_zeroconf_scanner_detector.get();
    lorgnette_scanner_manager_ = LorgnetteScannerManager::Create(
        std::move(fake_zeroconf_scanner_detector));
  }

  ~LorgnetteScannerManagerTest() override { DBusThreadManager::Shutdown(); }

  FakeLorgnetteManagerClient* GetLorgnetteManagerClient() {
    return static_cast<FakeLorgnetteManagerClient*>(
        DBusThreadManager::Get()->GetLorgnetteManagerClient());
  }

  // Calls LorgnetteScannerManager::GetScannerNames() and binds a callback to
  // process the result.
  void GetScannerNames() {
    lorgnette_scanner_manager_->GetScannerNames(
        base::Bind(&LorgnetteScannerManagerTest::GetScannerNamesCallback,
                   base::Unretained(this)));
  }

  // Calls LorgnetteScannerManager::Scan() and binds a callback to process the
  // result.
  void Scan(const std::string& scanner_name,
            const LorgnetteManagerClient::ScanProperties& scan_properties) {
    lorgnette_scanner_manager_->Scan(
        scanner_name, scan_properties,
        base::Bind(&LorgnetteScannerManagerTest::ScanCallback,
                   base::Unretained(this)));
  }

  // Runs all tasks until the ThreadPool's non-delayed queues are empty.
  void CompleteTasks() { task_environment_.RunUntilIdle(); }

  // Runs run_loop_ until a callback calls Quit().
  void WaitForResult() {
    run_loop_->Run();
    run_loop_.reset(new base::RunLoop());
  }

  FakeZeroconfScannerDetector* fake_zeroconf_scanner_detector() {
    return fake_zeroconf_scanner_detector_;
  }

  const std::vector<std::string>& scanner_names() const {
    return scanner_names_;
  }

  base::Optional<std::string> scan_data() const { return scan_data_; }

 private:
  // Handles the result of calling LorgnetteScannerManager::GetScannerNames().
  void GetScannerNamesCallback(std::vector<std::string> scanner_names) {
    scanner_names_ = scanner_names;
    run_loop_->Quit();
  }

  // Handles the result of calling LorgnetteScannerManager::Scan().
  void ScanCallback(base::Optional<std::string> scan_data) {
    scan_data_ = scan_data;
    run_loop_->Quit();
  }

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<base::RunLoop> run_loop_;

  FakeZeroconfScannerDetector* fake_zeroconf_scanner_detector_;

  std::unique_ptr<LorgnetteScannerManager> lorgnette_scanner_manager_;

  std::vector<std::string> scanner_names_;
  base::Optional<std::string> scan_data_;
};

// Test that no scanner names are returned when no scanners have been detected.
TEST_F(LorgnetteScannerManagerTest, NoScanners) {
  GetScannerNames();
  WaitForResult();
  EXPECT_TRUE(scanner_names().empty());
}

// Test that the name of a detected zeroconf scanner can be retrieved.
TEST_F(LorgnetteScannerManagerTest, ZeroconfScanner) {
  auto scanner = CreateZeroconfScanner();
  fake_zeroconf_scanner_detector()->AddDetections({scanner});
  CompleteTasks();
  GetScannerNames();
  WaitForResult();
  EXPECT_THAT(scanner_names(), ElementsAreArray({scanner.display_name}));
}

// Test that the name of a detected lorgnette scanner can be retrieved.
TEST_F(LorgnetteScannerManagerTest, LorgnetteScanner) {
  lorgnette::ListScannersResponse response =
      CreateListScannersResponse(kLorgnetteNetworkIpDeviceName);
  GetLorgnetteManagerClient()->SetListScannersResponse(response);
  GetScannerNames();
  WaitForResult();
  const auto& scanner = response.scanners()[0];
  std::string scanner_name = scanner.manufacturer() + " " + scanner.model();
  EXPECT_THAT(scanner_names(), ElementsAreArray({scanner_name}));
}

// Test that two detected scanners with the same IP address are deduplicated and
// reported with single scanner name.
TEST_F(LorgnetteScannerManagerTest, DeduplicateScanner) {
  GetLorgnetteManagerClient()->SetListScannersResponse(
      CreateListScannersResponse(kLorgnetteNetworkIpDeviceName));
  auto scanner = CreateZeroconfScanner();
  fake_zeroconf_scanner_detector()->AddDetections({scanner});
  CompleteTasks();
  GetScannerNames();
  WaitForResult();
  EXPECT_THAT(scanner_names(), ElementsAreArray({scanner.display_name}));
}

// Test that a lorgnette scanner with a URL in the name gets reported as a
// network scanner instead of a USB scanner (i.e. USB is not in the returned
// scanner name).
TEST_F(LorgnetteScannerManagerTest, LorgnetteScannerWithUrl) {
  lorgnette::ListScannersResponse response =
      CreateListScannersResponse(kLorgnetteNetworkUrlDeviceName);
  GetLorgnetteManagerClient()->SetListScannersResponse(response);
  GetScannerNames();
  WaitForResult();
  auto& scanner = response.scanners()[0];
  std::string scanner_name = scanner.manufacturer() + " " + scanner.model();
  EXPECT_THAT(scanner_names(), ElementsAreArray({scanner_name}));
}

// Test that detecting a lorgnette USB scanner results in a scanner name ending
// with "(USB)."
TEST_F(LorgnetteScannerManagerTest, LorgnetteUSBScanner) {
  lorgnette::ListScannersResponse response =
      CreateListScannersResponse(kLorgnetteUsbDeviceName);
  GetLorgnetteManagerClient()->SetListScannersResponse(response);
  GetScannerNames();
  WaitForResult();
  auto& scanner = response.scanners()[0];
  std::string scanner_name =
      scanner.manufacturer() + " " + scanner.model() + " (USB)";
  EXPECT_THAT(scanner_names(), ElementsAreArray({scanner_name}));
}

// Test that two lorgnette scanners with the same manufacturer and model are
// given unique names.
TEST_F(LorgnetteScannerManagerTest, UniqueScannerNames) {
  lorgnette::ListScannersResponse response =
      CreateListScannersResponse(kLorgnetteNetworkIpDeviceName);
  lorgnette::ScannerInfo scanner =
      CreateLorgnetteScanner(kLorgnetteNetworkIpDeviceName);
  *response.add_scanners() = std::move(scanner);
  GetLorgnetteManagerClient()->SetListScannersResponse(response);
  GetScannerNames();
  WaitForResult();
  ASSERT_EQ(scanner_names().size(), 2ul);
  EXPECT_NE(scanner_names()[0], scanner_names()[1]);
}

// Test that removing a detected scanner removes it from the list of available
// scanners.
TEST_F(LorgnetteScannerManagerTest, RemoveScanner) {
  auto scanner = CreateZeroconfScanner();
  fake_zeroconf_scanner_detector()->AddDetections({scanner});
  CompleteTasks();
  GetScannerNames();
  WaitForResult();
  EXPECT_THAT(scanner_names(), ElementsAreArray({scanner.display_name}));
  fake_zeroconf_scanner_detector()->RemoveDetections({scanner});
  CompleteTasks();
  GetScannerNames();
  WaitForResult();
  EXPECT_TRUE(scanner_names().empty());
}

// Test that scanning fails when GetScannerNames() has never been called.
TEST_F(LorgnetteScannerManagerTest, NoScannersNames) {
  chromeos::LorgnetteManagerClient::ScanProperties properties;
  Scan(kUnknownScannerName, properties);
  WaitForResult();
  EXPECT_FALSE(scan_data());
}

// Test that scanning fails when the scanner name does not correspond to a known
// scanner.
TEST_F(LorgnetteScannerManagerTest, UnknownScannerName) {
  fake_zeroconf_scanner_detector()->AddDetections({CreateZeroconfScanner()});
  CompleteTasks();
  GetScannerNames();
  WaitForResult();
  chromeos::LorgnetteManagerClient::ScanProperties properties;
  Scan(kUnknownScannerName, properties);
  WaitForResult();
  EXPECT_FALSE(scan_data());
}

// Test that scanning fails when there is no usable device name.
TEST_F(LorgnetteScannerManagerTest, NoUsableDeviceName) {
  auto scanner = CreateZeroconfScanner(/*usable=*/false);
  fake_zeroconf_scanner_detector()->AddDetections({scanner});
  CompleteTasks();
  GetScannerNames();
  WaitForResult();
  chromeos::LorgnetteManagerClient::ScanProperties properties;
  Scan(scanner.display_name, properties);
  WaitForResult();
  EXPECT_FALSE(scan_data());
}

// Test that scanning succeeds with a valid scanner name.
TEST_F(LorgnetteScannerManagerTest, Scan) {
  auto scanner = CreateZeroconfScanner();
  fake_zeroconf_scanner_detector()->AddDetections({scanner});
  CompleteTasks();
  GetScannerNames();
  WaitForResult();
  GetLorgnetteManagerClient()->SetScanResponse("TestScanData");
  chromeos::LorgnetteManagerClient::ScanProperties properties;
  Scan(scanner.display_name, properties);
  WaitForResult();
  ASSERT_TRUE(scan_data());
  EXPECT_EQ(scan_data().value(), "TestScanData");
}

}  // namespace chromeos
