// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/scanning/scan_service.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chrome/browser/chromeos/scanning/fake_lorgnette_scanner_manager.h"
#include "chromeos/components/scanning/mojom/scanning.mojom-test-utils.h"
#include "chromeos/components/scanning/mojom/scanning.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

namespace mojo_ipc = scanning::mojom;

// Scanner names used for tests.
constexpr char kFirstTestScannerName[] = "Test Scanner 1";
constexpr char kSecondTestScannerName[] = "Test Scanner 2";

}  // namespace

class ScanServiceTest : public testing::Test {
 public:
  ScanServiceTest() = default;

  void SetUp() override {
    scan_service_.BindInterface(
        scan_service_remote_.BindNewPipeAndPassReceiver());
  }

  // Gets scanners by calling ScanService::GetScanners() via the mojo::Remote.
  std::vector<mojo_ipc::ScannerPtr> GetScanners() {
    std::vector<mojo_ipc::ScannerPtr> scanners;
    mojo_ipc::ScanServiceAsyncWaiter(scan_service_remote_.get())
        .GetScanners(&scanners);
    return scanners;
  }

 protected:
  FakeLorgnetteScannerManager fake_lorgnette_scanner_manager_;

 private:
  base::test::TaskEnvironment task_environment_;
  ScanService scan_service_{&fake_lorgnette_scanner_manager_};
  mojo::Remote<mojo_ipc::ScanService> scan_service_remote_;
};

// Test that no scanners are returned when there are no scanner names.
TEST_F(ScanServiceTest, NoScannerNames) {
  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse({});
  auto scanners = GetScanners();
  EXPECT_TRUE(scanners.empty());
}

// Test that a scanner is returned with the correct display name.
TEST_F(ScanServiceTest, GetScanners) {
  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse(
      {kFirstTestScannerName});
  auto scanners = GetScanners();
  ASSERT_EQ(scanners.size(), 1u);
  EXPECT_EQ(scanners[0]->display_name,
            base::UTF8ToUTF16(kFirstTestScannerName));
}

// Test that two returned scanners have unique IDs.
TEST_F(ScanServiceTest, UniqueScannerIds) {
  fake_lorgnette_scanner_manager_.SetGetScannerNamesResponse(
      {kFirstTestScannerName, kSecondTestScannerName});
  auto scanners = GetScanners();
  ASSERT_EQ(scanners.size(), 2u);
  EXPECT_EQ(scanners[0]->display_name,
            base::UTF8ToUTF16(kFirstTestScannerName));
  EXPECT_EQ(scanners[1]->display_name,
            base::UTF8ToUTF16(kSecondTestScannerName));
  EXPECT_NE(scanners[0]->id, scanners[1]->id);
}

}  // namespace chromeos
