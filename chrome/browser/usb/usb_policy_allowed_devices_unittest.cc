// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_policy_allowed_devices.h"

#include <string>
#include <utility>

#include "base/json/json_reader.h"
#include "chrome/browser/policy/webusb_allow_devices_for_urls_policy_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

// For ChromeOS this test is parameterized to test user and device policy. For
// other operating systems, this test just tests the user policy.
enum class PolicyType { kUser, kDevice };

class UsbPolicyAllowedDevicesTestBase
    : public testing::Test,
      public testing::WithParamInterface<PolicyType> {
 public:
  UsbPolicyAllowedDevicesTestBase() {
#if defined(OS_CHROMEOS)
    policy::WebUsbAllowDevicesForUrlsPolicyHandler::RegisterPrefs(
        local_state()->registry());
#endif  // defined(OS_CHROMEOS)
  }

  TestingPrefServiceSimple* local_state() { return &local_state_; }

  std::unique_ptr<UsbPolicyAllowedDevices> Create() {
    return std::make_unique<UsbPolicyAllowedDevices>(profile()->GetPrefs(),
                                                     local_state());
  }

 protected:
  Profile* profile() { return &profile_; }

  content::BrowserTaskEnvironment task_environment_;

  device::FakeUsbDeviceManager device_manager_;

  TestingProfile profile_;
  TestingPrefServiceSimple local_state_;

  DISALLOW_COPY_AND_ASSIGN(UsbPolicyAllowedDevicesTestBase);
};

class UsbPolicyAllowedDevicesTest : public UsbPolicyAllowedDevicesTestBase {
 public:
  virtual void SetWebUsbAllowDevicesForUrlsPrefValue(const base::Value& value) {
#if defined(OS_CHROMEOS)
    if (GetParam() == PolicyType::kDevice) {
      local_state_.Set(prefs::kDeviceWebUsbAllowDevicesForUrls, value);
      return;
    }
#endif  // defined(OS_CHROMEOS)
    profile_.GetPrefs()->Set(prefs::kManagedWebUsbAllowDevicesForUrls, value);
  }
};

}  // namespace

TEST_P(UsbPolicyAllowedDevicesTest, InitializeWithMissingPrefValue) {
  auto usb_policy_allowed_devices = Create();

  EXPECT_TRUE(usb_policy_allowed_devices->map().empty());
}

TEST_P(UsbPolicyAllowedDevicesTest, InitializeWithExistingEmptyPrefValue) {
  base::Value pref_value(base::Value::Type::LIST);

  SetWebUsbAllowDevicesForUrlsPrefValue(pref_value);

  auto usb_policy_allowed_devices = Create();

  EXPECT_TRUE(usb_policy_allowed_devices->map().empty());
}

namespace {

constexpr char kPolicySetting[] = R"(
    [
      {
        "devices": [
          { "vendor_id": 1234, "product_id": 5678 },
          { "vendor_id": 4321 }
        ],
        "urls": [
          "https://google.com,https://google.com",
          "https://crbug.com"
        ]
      }, {
        "devices": [{}],
        "urls": ["https://www.youtube.com"]
      }
    ])";

std::pair<url::Origin, base::Optional<url::Origin>> MakeOriginPair(
    std::string requesting_url) {
  return std::make_pair(url::Origin::Create(GURL(requesting_url)),
                        base::nullopt);
}

std::pair<url::Origin, base::Optional<url::Origin>> MakeOriginPair(
    std::string requesting_url,
    std::string embedding_url) {
  return std::make_pair(url::Origin::Create(GURL(requesting_url)),
                        url::Origin::Create(GURL(embedding_url)));
}

}  // namespace

TEST_P(UsbPolicyAllowedDevicesTest, InitializeWithExistingPrefValue) {
  std::unique_ptr<base::Value> pref_value =
      base::JSONReader::ReadDeprecated(kPolicySetting);

  SetWebUsbAllowDevicesForUrlsPrefValue(*pref_value);

  auto usb_policy_allowed_devices = Create();

  const UsbPolicyAllowedDevices::UsbDeviceIdsToUrlsMap& map =
      usb_policy_allowed_devices->map();
  EXPECT_EQ(map.size(), 3ul);

  auto device_key = std::make_pair(1234, 5678);
  ASSERT_TRUE(base::Contains(map, device_key));

  const auto& first_urls = map.at(device_key);
  EXPECT_TRUE(base::Contains(
      first_urls, MakeOriginPair("https://google.com", "https://google.com")));
  EXPECT_TRUE(base::Contains(first_urls, MakeOriginPair("https://crbug.com")));

  device_key = std::make_pair(4321, -1);
  ASSERT_TRUE(base::Contains(map, device_key));

  const auto& second_urls = map.at(device_key);
  EXPECT_TRUE(base::Contains(
      second_urls, MakeOriginPair("https://google.com", "https://google.com")));
  EXPECT_TRUE(base::Contains(second_urls, MakeOriginPair("https://crbug.com")));

  device_key = std::make_pair(-1, -1);
  ASSERT_TRUE(base::Contains(map, device_key));

  const auto& third_urls = map.at(device_key);
  EXPECT_TRUE(
      base::Contains(third_urls, MakeOriginPair("https://www.youtube.com")));
}

TEST_P(UsbPolicyAllowedDevicesTest,
       InitializeWithMissingPolicyThenUpdatePolicy) {
  auto usb_policy_allowed_devices = Create();
  EXPECT_TRUE(usb_policy_allowed_devices->map().empty());

  // Ensure that the allowed devices can be dynamically updated.
  std::unique_ptr<base::Value> pref_value =
      base::JSONReader::ReadDeprecated(kPolicySetting);

  SetWebUsbAllowDevicesForUrlsPrefValue(*pref_value);

  const UsbPolicyAllowedDevices::UsbDeviceIdsToUrlsMap& map =
      usb_policy_allowed_devices->map();
  EXPECT_EQ(map.size(), 3ul);

  auto device_key = std::make_pair(1234, 5678);
  ASSERT_TRUE(base::Contains(map, device_key));

  const auto& first_urls = map.at(device_key);
  EXPECT_TRUE(base::Contains(
      first_urls, MakeOriginPair("https://google.com", "https://google.com")));
  EXPECT_TRUE(base::Contains(first_urls, MakeOriginPair("https://crbug.com")));

  device_key = std::make_pair(4321, -1);
  ASSERT_TRUE(base::Contains(map, device_key));

  const auto& second_urls = map.at(device_key);
  EXPECT_TRUE(base::Contains(
      second_urls, MakeOriginPair("https://google.com", "https://google.com")));
  EXPECT_TRUE(base::Contains(second_urls, MakeOriginPair("https://crbug.com")));

  device_key = std::make_pair(-1, -1);
  ASSERT_TRUE(base::Contains(map, device_key));

  const auto& third_urls = map.at(device_key);
  EXPECT_TRUE(
      base::Contains(third_urls, MakeOriginPair("https://www.youtube.com")));
}

TEST_P(UsbPolicyAllowedDevicesTest,
       InitializeWithExistingPolicyThenRemovePolicy) {
  std::unique_ptr<base::Value> pref_value =
      base::JSONReader::ReadDeprecated(kPolicySetting);

  SetWebUsbAllowDevicesForUrlsPrefValue(*pref_value);

  auto usb_policy_allowed_devices = Create();

  const UsbPolicyAllowedDevices::UsbDeviceIdsToUrlsMap& map =
      usb_policy_allowed_devices->map();
  EXPECT_EQ(map.size(), 3ul);

  auto device_key = std::make_pair(1234, 5678);
  ASSERT_TRUE(base::Contains(map, device_key));

  const auto& first_urls = map.at(device_key);
  EXPECT_TRUE(base::Contains(
      first_urls, MakeOriginPair("https://google.com", "https://google.com")));
  EXPECT_TRUE(base::Contains(first_urls, MakeOriginPair("https://crbug.com")));

  device_key = std::make_pair(4321, -1);
  ASSERT_TRUE(base::Contains(map, device_key));

  const auto& second_urls = map.at(device_key);
  EXPECT_TRUE(base::Contains(
      second_urls, MakeOriginPair("https://google.com", "https://google.com")));
  EXPECT_TRUE(base::Contains(second_urls, MakeOriginPair("https://crbug.com")));

  device_key = std::make_pair(-1, -1);
  ASSERT_TRUE(base::Contains(map, device_key));

  const auto& third_urls = map.at(device_key);
  EXPECT_TRUE(
      base::Contains(third_urls, MakeOriginPair("https://www.youtube.com")));

  // Ensure that the allowed devices can be removed dynamically.
  pref_value.reset(new base::Value(base::Value::Type::LIST));
  SetWebUsbAllowDevicesForUrlsPrefValue(*pref_value);

  EXPECT_TRUE(usb_policy_allowed_devices->map().empty());
}

namespace {

constexpr char kPolicySettingWithEntriesContainingDuplicateDevices[] = R"(
    [
      {
        "devices": [{ "vendor_id": 1234, "product_id": 5678 }],
        "urls": [
          "https://google.com",
          "https://crbug.com"
        ]
      }, {
        "devices": [{ "vendor_id": 1234, "product_id": 5678 }],
        "urls": ["https://www.youtube.com"]
      }
    ])";

}  // namespace

TEST_P(UsbPolicyAllowedDevicesTest,
       InitializeWithExistingPrefValueContainingDuplicateDevices) {
  std::unique_ptr<base::Value> pref_value = base::JSONReader::ReadDeprecated(
      kPolicySettingWithEntriesContainingDuplicateDevices);

  SetWebUsbAllowDevicesForUrlsPrefValue(*pref_value);

  auto usb_policy_allowed_devices = Create();

  const UsbPolicyAllowedDevices::UsbDeviceIdsToUrlsMap& map =
      usb_policy_allowed_devices->map();
  ASSERT_EQ(map.size(), 1ul);

  auto device_key = std::make_pair(1234, 5678);
  ASSERT_TRUE(base::Contains(map, device_key));

  // Ensure a device has all of the URL patterns allowed to access it.
  const auto& urls = map.at(device_key);
  EXPECT_TRUE(base::Contains(urls, MakeOriginPair("https://google.com")));
  EXPECT_TRUE(base::Contains(urls, MakeOriginPair("https://crbug.com")));
  EXPECT_TRUE(base::Contains(urls, MakeOriginPair("https://www.youtube.com")));
}

namespace {

constexpr char kPolicySettingWithEntriesMatchingMultipleDevices[] = R"(
    [
      {
        "devices": [{ "vendor_id": 1234, "product_id": 5678 }],
        "urls": ["https://google.com"]
      }, {
        "devices": [{ "vendor_id": 1234 }],
        "urls": ["https://www.youtube.com"]
      }, {
        "devices": [{}],
        "urls": ["https://chromium.org"]
      }
    ])";

}  // namespace

TEST_P(UsbPolicyAllowedDevicesTest, IsDeviceAllowed) {
  std::unique_ptr<base::Value> pref_value = base::JSONReader::ReadDeprecated(
      kPolicySettingWithEntriesMatchingMultipleDevices);

  SetWebUsbAllowDevicesForUrlsPrefValue(*pref_value);

  auto usb_policy_allowed_devices = Create();

  const auto kGoogleOrigin = url::Origin::Create(GURL("https://google.com"));
  const auto kYoutubeOrigin =
      url::Origin::Create(GURL("https://www.youtube.com"));
  const auto kChromiumOrigin =
      url::Origin::Create(GURL("https://chromium.org"));
  const auto kAndroidOrigin = url::Origin::Create(GURL("https://android.com"));

  auto specific_device_info = device_manager_.CreateAndAddDevice(
      1234, 5678, "Google", "Gizmo", "123ABC");
  auto vendor_device_info = device_manager_.CreateAndAddDevice(
      1234, 8765, "Google", "Gizmo", "ABC123");
  auto unrelated_device_info = device_manager_.CreateAndAddDevice(
      4321, 8765, "Chrome", "Gizmo", "987ZYX");

  // Check that the specific device is allowed for https://google.com embedded
  // in any origin, but not any other device.
  EXPECT_TRUE(usb_policy_allowed_devices->IsDeviceAllowed(
      kGoogleOrigin, kGoogleOrigin, *specific_device_info));
  EXPECT_TRUE(usb_policy_allowed_devices->IsDeviceAllowed(
      kGoogleOrigin, kAndroidOrigin, *specific_device_info));
  EXPECT_FALSE(usb_policy_allowed_devices->IsDeviceAllowed(
      kGoogleOrigin, kGoogleOrigin, *vendor_device_info));
  EXPECT_FALSE(usb_policy_allowed_devices->IsDeviceAllowed(
      kGoogleOrigin, kAndroidOrigin, *vendor_device_info));
  EXPECT_FALSE(usb_policy_allowed_devices->IsDeviceAllowed(
      kGoogleOrigin, kGoogleOrigin, *unrelated_device_info));
  EXPECT_FALSE(usb_policy_allowed_devices->IsDeviceAllowed(
      kGoogleOrigin, kAndroidOrigin, *unrelated_device_info));

  // Check that devices with a vendor ID of 1234 are allowed for
  // https://www.youtube.com embedded in any origin, but not an unrelated
  // device.
  EXPECT_TRUE(usb_policy_allowed_devices->IsDeviceAllowed(
      kYoutubeOrigin, kYoutubeOrigin, *specific_device_info));
  EXPECT_TRUE(usb_policy_allowed_devices->IsDeviceAllowed(
      kYoutubeOrigin, kAndroidOrigin, *specific_device_info));
  EXPECT_TRUE(usb_policy_allowed_devices->IsDeviceAllowed(
      kYoutubeOrigin, kYoutubeOrigin, *vendor_device_info));
  EXPECT_TRUE(usb_policy_allowed_devices->IsDeviceAllowed(
      kYoutubeOrigin, kAndroidOrigin, *vendor_device_info));
  EXPECT_FALSE(usb_policy_allowed_devices->IsDeviceAllowed(
      kYoutubeOrigin, kYoutubeOrigin, *unrelated_device_info));
  EXPECT_FALSE(usb_policy_allowed_devices->IsDeviceAllowed(
      kYoutubeOrigin, kAndroidOrigin, *unrelated_device_info));

  // Check that any device is allowed for https://chromium.org embedded in any
  // origin.
  EXPECT_TRUE(usb_policy_allowed_devices->IsDeviceAllowed(
      kChromiumOrigin, kChromiumOrigin, *specific_device_info));
  EXPECT_TRUE(usb_policy_allowed_devices->IsDeviceAllowed(
      kChromiumOrigin, kAndroidOrigin, *specific_device_info));
  EXPECT_TRUE(usb_policy_allowed_devices->IsDeviceAllowed(
      kChromiumOrigin, kChromiumOrigin, *vendor_device_info));
  EXPECT_TRUE(usb_policy_allowed_devices->IsDeviceAllowed(
      kChromiumOrigin, kAndroidOrigin, *vendor_device_info));
  EXPECT_TRUE(usb_policy_allowed_devices->IsDeviceAllowed(
      kChromiumOrigin, kChromiumOrigin, *unrelated_device_info));
  EXPECT_TRUE(usb_policy_allowed_devices->IsDeviceAllowed(
      kChromiumOrigin, kAndroidOrigin, *unrelated_device_info));
}

TEST_P(UsbPolicyAllowedDevicesTest, IsDeviceAllowedForUrlsNotInPref) {
  std::unique_ptr<base::Value> pref_value = base::JSONReader::ReadDeprecated(
      kPolicySettingWithEntriesMatchingMultipleDevices);

  SetWebUsbAllowDevicesForUrlsPrefValue(*pref_value);

  auto usb_policy_allowed_devices = Create();

  const url::Origin origins[] = {
      url::Origin::Create(GURL("https://evil.com")),
      url::Origin::Create(GURL("https://very.evil.com")),
      url::Origin::Create(GURL("https://chromium.deceptive.org"))};

  auto device_info = device_manager_.CreateAndAddDevice(1234, 5678, "Google",
                                                        "Gizmo", "123ABC");
  for (const url::Origin& requesting_origin : origins) {
    for (const url::Origin& embedding_origin : origins) {
      EXPECT_FALSE(usb_policy_allowed_devices->IsDeviceAllowed(
          requesting_origin, embedding_origin, *device_info));
    }
  }
}

TEST_P(UsbPolicyAllowedDevicesTest, IsDeviceAllowedForDeviceNotInPref) {
  std::unique_ptr<base::Value> pref_value = base::JSONReader::ReadDeprecated(
      kPolicySettingWithEntriesMatchingMultipleDevices);

  SetWebUsbAllowDevicesForUrlsPrefValue(*pref_value);

  auto usb_policy_allowed_devices = Create();

  const url::Origin origins[] = {
      url::Origin::Create(GURL("https://google.com")),
      url::Origin::Create(GURL("https://www.youtube.com"))};

  auto device_info = device_manager_.CreateAndAddDevice(4321, 8765, "Google",
                                                        "Gizmo", "123ABC");
  for (const url::Origin& requesting_origin : origins) {
    for (const url::Origin& embedding_origin : origins) {
      EXPECT_FALSE(usb_policy_allowed_devices->IsDeviceAllowed(
          requesting_origin, embedding_origin, *device_info));
    }
  }
}

namespace {

constexpr char kPolicySettingWithUrlContainingEmbeddingOrigin[] = R"(
    [
      {
        "devices": [{ "vendor_id": 1234, "product_id": 5678 }],
        "urls": [
          "https://requesting.com,https://embedding.com"
        ]
      }
    ])";

}  // namespace

TEST_P(UsbPolicyAllowedDevicesTest,
       IsDeviceAllowedForUrlContainingEmbeddingOrigin) {
  std::unique_ptr<base::Value> pref_value = base::JSONReader::ReadDeprecated(
      kPolicySettingWithUrlContainingEmbeddingOrigin);

  SetWebUsbAllowDevicesForUrlsPrefValue(*pref_value);

  auto usb_policy_allowed_devices = Create();

  const auto requesting_origin =
      url::Origin::Create(GURL("https://requesting.com"));
  const auto embedding_origin =
      url::Origin::Create(GURL("https://embedding.com"));

  auto device_info = device_manager_.CreateAndAddDevice(1234, 5678, "Google",
                                                        "Gizmo", "123ABC");
  EXPECT_TRUE(usb_policy_allowed_devices->IsDeviceAllowed(
      requesting_origin, embedding_origin, *device_info));
  EXPECT_FALSE(usb_policy_allowed_devices->IsDeviceAllowed(
      embedding_origin, requesting_origin, *device_info));
  EXPECT_FALSE(usb_policy_allowed_devices->IsDeviceAllowed(
      requesting_origin, requesting_origin, *device_info));
  EXPECT_FALSE(usb_policy_allowed_devices->IsDeviceAllowed(
      embedding_origin, embedding_origin, *device_info));
}

// For ChromeOS this test is parameterized to test user and device policy. For
// other operating systems, this test just tests the user policy.
INSTANTIATE_TEST_SUITE_P(,
                         UsbPolicyAllowedDevicesTest,
#if defined(OS_CHROMEOS)
                         testing::Values(PolicyType::kUser, PolicyType::kDevice)
#else
                         testing::Values(PolicyType::kUser)
#endif
);

#if defined(OS_CHROMEOS)
namespace {

constexpr char kUserPolicySetting[] = R"(
    [
      {
        "devices": [
          { "vendor_id": 1234, "product_id": 5678 },
          { "vendor_id": 4321 }
        ],
        "urls": [
          "https://crbug.com"
        ]
      }, {
        "devices": [{}],
        "urls": ["https://www.youtube.com"]
      }
    ])";

constexpr char kDevicePolicySetting[] = R"(
    [
      {
        "devices": [
          { "vendor_id": 1234, "product_id": 5678 }
        ],
        "urls": [
          "https://google.com"
        ]
      }, {
        "devices": [{ "vendor_id": 1111, "product_id": 2222 }],
        "urls": ["https://www.ebay.com"]
      }
    ])";

}  // namespace

TEST_P(UsbPolicyAllowedDevicesTestBase, CombineUserAndDevicePolicies) {
  // Set user policy pref directly.
  std::unique_ptr<base::Value> user_pref_value =
      base::JSONReader::ReadDeprecated(kUserPolicySetting);
  profile_.GetPrefs()->Set(prefs::kManagedWebUsbAllowDevicesForUrls,
                           *user_pref_value);

  // Set device policy pref directly.
  std::unique_ptr<base::Value> device_pref_value =
      base::JSONReader::ReadDeprecated(kDevicePolicySetting);
  local_state_.Set(prefs::kDeviceWebUsbAllowDevicesForUrls, *device_pref_value);

  auto usb_policy_allowed_devices = Create();

  const UsbPolicyAllowedDevices::UsbDeviceIdsToUrlsMap& map =
      usb_policy_allowed_devices->map();
  EXPECT_EQ(map.size(), 4ul);

  auto device_key = std::make_pair(1234, 5678);
  ASSERT_TRUE(base::Contains(map, device_key));
  const auto& first_urls = map.at(device_key);
  EXPECT_TRUE(base::Contains(first_urls, MakeOriginPair("https://crbug.com")));
  EXPECT_TRUE(base::Contains(first_urls, MakeOriginPair("https://google.com")));

  device_key = std::make_pair(4321, -1);
  ASSERT_TRUE(base::Contains(map, device_key));
  const auto& second_urls = map.at(device_key);
  EXPECT_TRUE(base::Contains(second_urls, MakeOriginPair("https://crbug.com")));
  EXPECT_FALSE(
      base::Contains(second_urls, MakeOriginPair("https://google.com")));

  device_key = std::make_pair(-1, -1);
  ASSERT_TRUE(base::Contains(map, device_key));
  const auto& third_urls = map.at(device_key);
  EXPECT_TRUE(
      base::Contains(third_urls, MakeOriginPair("https://www.youtube.com")));

  device_key = std::make_pair(1111, 2222);
  ASSERT_TRUE(base::Contains(map, device_key));
  const auto& fourth_urls = map.at(device_key);
  EXPECT_TRUE(
      base::Contains(fourth_urls, MakeOriginPair("https://www.ebay.com")));
}
#endif  // defined(OS_CHROMEOS)
