// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/telemetry_extension_ui/test/telemetry_extension_ui_browsertest.h"

#include "base/files/file_path.h"
#include "chromeos/components/telemetry_extension_ui/url_constants.h"
#include "chromeos/components/web_applications/test/sandboxed_web_ui_test_base.h"
#include "chromeos/dbus/cros_healthd/cros_healthd_client.h"
#include "chromeos/dbus/cros_healthd/fake_cros_healthd_client.h"

namespace {

// File with utility functions for testing, defines `test_util`.
constexpr base::FilePath::CharType kWebUiTestUtil[] =
    FILE_PATH_LITERAL("chrome/test/data/webui/test_util.js");

// File that `kWebUiTestUtil` is dependent on, defines `cr`.
constexpr base::FilePath::CharType kCr[] =
    FILE_PATH_LITERAL("ui/webui/resources/js/cr.js");

// File containing the query handlers for JS unit tests.
constexpr base::FilePath::CharType kUntrustedTestHandlers[] = FILE_PATH_LITERAL(
    "chromeos/components/telemetry_extension_ui/test/"
    "untrusted_test_handlers.js");

// Test cases that run in the untrusted context.
constexpr base::FilePath::CharType kUntrustedTestCases[] = FILE_PATH_LITERAL(
    "chromeos/components/telemetry_extension_ui/test/untrusted_browsertest.js");

}  // namespace

TelemetryExtensionUiBrowserTest::TelemetryExtensionUiBrowserTest()
    : SandboxedWebUiAppTestBase(
          chromeos::kChromeUITelemetryExtensionURL,
          chromeos::kChromeUIUntrustedTelemetryExtensionURL,
          {base::FilePath(kCr), base::FilePath(kWebUiTestUtil),
           base::FilePath(kUntrustedTestHandlers),
           base::FilePath(kUntrustedTestCases)}) {}

TelemetryExtensionUiBrowserTest::~TelemetryExtensionUiBrowserTest() = default;

void TelemetryExtensionUiBrowserTest::SetUpOnMainThread() {
  {
    namespace cros_diagnostics = ::chromeos::cros_healthd::mojom;

    std::vector<cros_diagnostics::DiagnosticRoutineEnum> input{
        cros_diagnostics::DiagnosticRoutineEnum::kBatteryCapacity,
        cros_diagnostics::DiagnosticRoutineEnum::kBatteryHealth,
        cros_diagnostics::DiagnosticRoutineEnum::kUrandom,
        cros_diagnostics::DiagnosticRoutineEnum::kSmartctlCheck,
        cros_diagnostics::DiagnosticRoutineEnum::kAcPower,
        cros_diagnostics::DiagnosticRoutineEnum::kCpuCache,
        cros_diagnostics::DiagnosticRoutineEnum::kCpuStress,
        cros_diagnostics::DiagnosticRoutineEnum::kFloatingPointAccuracy,
        cros_diagnostics::DiagnosticRoutineEnum::kNvmeWearLevel,
        cros_diagnostics::DiagnosticRoutineEnum::kNvmeSelfTest,
        cros_diagnostics::DiagnosticRoutineEnum::kDiskRead,
        cros_diagnostics::DiagnosticRoutineEnum::kPrimeSearch,
        cros_diagnostics::DiagnosticRoutineEnum::kBatteryDischarge,
    };

    chromeos::cros_healthd::FakeCrosHealthdClient::Get()
        ->SetAvailableRoutinesForTesting(input);
  }

  {
    auto battery_info = chromeos::cros_healthd::mojom::BatteryInfo::New();
    battery_info->cycle_count = 100000000000000;
    battery_info->voltage_now = 1234567890.123456;
    battery_info->vendor = "Google";
    battery_info->serial_number = "abcdef";
    battery_info->charge_full_design = 3000000000000000;
    battery_info->charge_full = 9000000000000000;
    battery_info->voltage_min_design = 1000000000.1001;
    battery_info->model_name = "Google Battery";
    battery_info->charge_now = 7777777777.777;
    battery_info->current_now = 0.9999999999999;
    battery_info->technology = "Li-ion";
    battery_info->status = "Charging";
    battery_info->manufacture_date = "2020-07-30";
    battery_info->temperature =
        chromeos::cros_healthd::mojom::UInt64Value::New(7777777777777777);

    auto info = chromeos::cros_healthd::mojom::TelemetryInfo::New();
    info->battery_result =
        chromeos::cros_healthd::mojom::BatteryResult::NewBatteryInfo(
            std::move(battery_info));

    DCHECK(chromeos::cros_healthd::FakeCrosHealthdClient::Get());

    chromeos::cros_healthd::FakeCrosHealthdClient::Get()
        ->SetProbeTelemetryInfoResponseForTesting(info);
  }

  SandboxedWebUiAppTestBase::SetUpOnMainThread();
}
