// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/calculators_policies_binder.h"

#include <array>
#include <string>

#include "base/test/task_environment.h"
#include "chrome/browser/chromeos/printing/bulk_printers_calculator.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

constexpr size_t kNumPrinters = 5;

constexpr size_t kWhitelistPrinters = 4;
constexpr std::array<const char*, kWhitelistPrinters> kWhitelistIds = {
    "First", "Second", "Third", "Fifth"};

constexpr std::array<const char*, 3> kBlacklistIds = {"Second", "Third",
                                                      "Fourth"};
// kNumPrinters - kBlacklistIds.size() = kBlacklistPrinters (2)
constexpr size_t kBlacklistPrinters = 2;

constexpr char kBulkPolicyContentsJson[] = R"json(
[
  {
    "id": "First",
    "display_name": "LexaPrint",
    "description": "Laser on the test shelf",
    "manufacturer": "LexaPrint, Inc.",
    "model": "MS610de",
    "uri": "ipp://192.168.1.5",
    "ppd_resource": {
      "effective_model": "MS610de"
    }
  }, {
    "id": "Second",
    "display_name": "Color Laser",
    "description": "The printer next to the water cooler.",
    "manufacturer": "Printer Manufacturer",
    "model":"Color Laser 2004",
    "uri":"ipps://print-server.intranet.example.com:443/ipp/cl2k4",
    "uuid":"1c395fdb-5d93-4904-b246-b2c046e79d12",
    "ppd_resource":{
      "effective_manufacturer": "MakesPrinters",
      "effective_model": "ColorLaser2k4"
    }
  }, {
    "id": "Third",
    "display_name": "YaLP",
    "description": "Fancy Fancy Fancy",
    "manufacturer": "LexaPrint, Inc.",
    "model": "MS610de",
    "uri": "ipp://192.168.1.8",
    "ppd_resource": {
      "effective_manufacturer": "LexaPrint",
      "effective_model": "MS610de"
    }
  }, {
    "id": "Fourth",
    "display_name": "Yon",
    "description": "Another printer",
    "manufacturer": "CrosPrints",
    "model": "1000d7",
    "uri": "ipp://192.168.1.9",
    "ppd_resource": {
      "effective_manufacturer": "Printer",
      "effective_model": "Model"
    }
  }, {
    "id": "Fifth",
    "display_name": "ABCDE",
    "description": "Yep yep yep",
    "manufacturer": "Ink and toner",
    "model": "34343434l",
    "uri": "ipp://192.168.1.10",
    "ppd_resource": {
      "effective_manufacturer": "Blah",
      "effective_model": "Blah blah Blah"
    }
  }
])json";

template <class Container>
std::unique_ptr<base::Value> StringsToList(Container container) {
  auto first = container.begin();
  auto last = container.end();
  auto list = std::make_unique<base::Value>(base::Value::Type::LIST);

  while (first != last) {
    list->Append(*first);
    first++;
  }
  return list;
}

class CalculatorsPoliciesBinderTest : public testing::Test {
 protected:
  void SetUp() override {
    CalculatorsPoliciesBinder::RegisterProfilePrefs(prefs_.registry());
  }

  std::unique_ptr<BulkPrintersCalculator> UserCalculator() {
    auto calculator = BulkPrintersCalculator::Create();
    binder_ =
        CalculatorsPoliciesBinder::UserBinder(&prefs_, calculator->AsWeakPtr());

    // Populate data
    calculator->SetData(std::make_unique<std::string>(kBulkPolicyContentsJson));
    return calculator;
  }

  std::unique_ptr<BulkPrintersCalculator> DeviceCalculator() {
    auto calculator = BulkPrintersCalculator::Create();
    binder_ = CalculatorsPoliciesBinder::DeviceBinder(CrosSettings::Get(),
                                                      calculator->AsWeakPtr());

    // Populate data
    calculator->SetData(std::make_unique<std::string>(kBulkPolicyContentsJson));
    return calculator;
  }

  void SetDeviceSetting(const std::string& path, const base::Value& value) {
    testing_settings_.device_settings()->Set(path, value);
  }

  base::test::TaskEnvironment env_;
  ScopedTestingCrosSettings testing_settings_;
  TestingPrefServiceSimple prefs_;

  // Class under test.  Expected to be destroyed first.
  std::unique_ptr<CalculatorsPoliciesBinder> binder_;
};

TEST_F(CalculatorsPoliciesBinderTest, PrefsAllAccess) {
  auto calculator = UserCalculator();

  // Set prefs to complete computation
  prefs_.SetManagedPref(prefs::kRecommendedNativePrintersAccessMode,
                        std::make_unique<base::Value>(
                            BulkPrintersCalculator::AccessMode::ALL_ACCESS));

  env_.RunUntilIdle();
  EXPECT_TRUE(calculator->IsComplete());
  EXPECT_EQ(calculator->GetPrinters().size(), kNumPrinters);
}

TEST_F(CalculatorsPoliciesBinderTest, PrefsWhitelist) {
  auto calculator = UserCalculator();

  // Set prefs to complete computation
  prefs_.SetManagedPref(
      prefs::kRecommendedNativePrintersAccessMode,
      std::make_unique<base::Value>(
          BulkPrintersCalculator::AccessMode::WHITELIST_ONLY));
  prefs_.SetManagedPref(prefs::kRecommendedNativePrintersWhitelist,
                        StringsToList(kWhitelistIds));

  env_.RunUntilIdle();
  EXPECT_TRUE(calculator->IsComplete());
  EXPECT_EQ(calculator->GetPrinters().size(), kWhitelistPrinters);
}

TEST_F(CalculatorsPoliciesBinderTest, PrefsBlacklist) {
  auto calculator = UserCalculator();

  // Set prefs to complete computation
  prefs_.SetManagedPref(
      prefs::kRecommendedNativePrintersAccessMode,
      std::make_unique<base::Value>(
          BulkPrintersCalculator::AccessMode::BLACKLIST_ONLY));
  prefs_.SetManagedPref(prefs::kRecommendedNativePrintersBlacklist,
                        StringsToList(kBlacklistIds));

  env_.RunUntilIdle();
  EXPECT_TRUE(calculator->IsComplete());
  EXPECT_EQ(calculator->GetPrinters().size(), kBlacklistPrinters);
}

TEST_F(CalculatorsPoliciesBinderTest, PrefsBeforeBind) {
  // Verify that if preferences are set before we bind to policies, the
  // calculator is still properly populated.
  prefs_.SetManagedPref(
      prefs::kRecommendedNativePrintersAccessMode,
      std::make_unique<base::Value>(
          BulkPrintersCalculator::AccessMode::WHITELIST_ONLY));
  prefs_.SetManagedPref(prefs::kRecommendedNativePrintersWhitelist,
                        StringsToList(kWhitelistIds));

  auto calculator = UserCalculator();

  env_.RunUntilIdle();
  EXPECT_TRUE(calculator->IsComplete());
  EXPECT_EQ(calculator->GetPrinters().size(), kWhitelistPrinters);
}

TEST_F(CalculatorsPoliciesBinderTest, SettingsAllAccess) {
  auto calculator = DeviceCalculator();

  SetDeviceSetting(kDeviceNativePrintersAccessMode,
                   base::Value(BulkPrintersCalculator::AccessMode::ALL_ACCESS));

  env_.RunUntilIdle();
  EXPECT_TRUE(calculator->IsComplete());
  EXPECT_EQ(calculator->GetPrinters().size(), kNumPrinters);
}

TEST_F(CalculatorsPoliciesBinderTest, SettingsWhitelist) {
  auto calculator = DeviceCalculator();

  SetDeviceSetting(
      kDeviceNativePrintersAccessMode,
      base::Value(BulkPrintersCalculator::AccessMode::WHITELIST_ONLY));
  SetDeviceSetting(kDeviceNativePrintersWhitelist,
                   *StringsToList(kWhitelistIds));

  env_.RunUntilIdle();
  EXPECT_TRUE(calculator->IsComplete());
  EXPECT_EQ(calculator->GetPrinters().size(), kWhitelistPrinters);
}

TEST_F(CalculatorsPoliciesBinderTest, SettingsBlacklist) {
  auto calculator = DeviceCalculator();

  SetDeviceSetting(
      kDeviceNativePrintersAccessMode,
      base::Value(BulkPrintersCalculator::AccessMode::BLACKLIST_ONLY));
  SetDeviceSetting(kDeviceNativePrintersBlacklist,
                   *StringsToList(kBlacklistIds));

  env_.RunUntilIdle();
  EXPECT_TRUE(calculator->IsComplete());
  EXPECT_EQ(calculator->GetPrinters().size(), kBlacklistPrinters);
}

TEST_F(CalculatorsPoliciesBinderTest, SettingsBeforeBind) {
  // Set policy before binding to the calculator.
  SetDeviceSetting(kDeviceNativePrintersAccessMode,
                   base::Value(BulkPrintersCalculator::AccessMode::ALL_ACCESS));

  auto calculator = DeviceCalculator();

  env_.RunUntilIdle();
  EXPECT_TRUE(calculator->IsComplete());
  EXPECT_EQ(calculator->GetPrinters().size(), kNumPrinters);
}

}  // namespace
}  // namespace chromeos
