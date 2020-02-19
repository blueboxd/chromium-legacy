// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_reporting_private/enterprise_reporting_private_api.h"

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/enterprise_reporting_private/chrome_desktop_report_request_helper.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/policy/fake_browser_dm_token_storage.h"
#include "chrome/common/extensions/api/enterprise_reporting_private.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

#if defined(OS_WIN)
#include "base/test/test_reg_util_win.h"
#endif

using ::testing::_;
using ::testing::Invoke;
using ::testing::WithArgs;

namespace enterprise_reporting_private =
    ::extensions::api::enterprise_reporting_private;

namespace extensions {
namespace {

const char kFakeDMToken[] = "fake-dm-token";
const char kFakeClientId[] = "fake-client-id";
const char kFakeMachineNameReport[] = "{\"computername\":\"name\"}";

}  // namespace

// Test for API enterprise.reportingPrivate.uploadChromeDesktopReport
class EnterpriseReportingPrivateUploadChromeDesktopReportTest
    : public ExtensionApiUnittest {
 public:
  EnterpriseReportingPrivateUploadChromeDesktopReportTest() {}

  ExtensionFunction* CreateChromeDesktopReportingFunction(
      const std::string& dm_token) {
    EnterpriseReportingPrivateUploadChromeDesktopReportFunction* function =
        EnterpriseReportingPrivateUploadChromeDesktopReportFunction::
            CreateForTesting(test_url_loader_factory_.GetSafeWeakWrapper());
    auto client = std::make_unique<policy::MockCloudPolicyClient>(
        test_url_loader_factory_.GetSafeWeakWrapper());
    client_ = client.get();
    function->SetCloudPolicyClientForTesting(std::move(client));
    if (dm_token.empty()) {
      function->SetRegistrationInfoForTesting(
          policy::DMToken::CreateEmptyTokenForTesting(), kFakeClientId);
    } else {
      function->SetRegistrationInfoForTesting(
          policy::DMToken::CreateValidTokenForTesting(dm_token), kFakeClientId);
    }
    return function;
  }

  std::string GenerateArgs(const char* name) {
    return base::StringPrintf("[{\"machineName\":%s}]", name);
  }

  std::string GenerateInvalidReport() {
    // This report is invalid as the chromeSignInUser dictionary should not be
    // wrapped in a list.
    return std::string(
        "[{\"browserReport\": "
        "{\"chromeUserProfileReport\":[{\"chromeSignInUser\":\"Name\"}]}}]");
  }

  policy::MockCloudPolicyClient* client_;

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(
      EnterpriseReportingPrivateUploadChromeDesktopReportTest);
};

TEST_F(EnterpriseReportingPrivateUploadChromeDesktopReportTest,
       DeviceIsNotEnrolled) {
  ASSERT_EQ(enterprise_reporting::kDeviceNotEnrolled,
            RunFunctionAndReturnError(
                CreateChromeDesktopReportingFunction(std::string()),
                GenerateArgs(kFakeMachineNameReport)));
}

TEST_F(EnterpriseReportingPrivateUploadChromeDesktopReportTest,
       ReportIsNotValid) {
  ASSERT_EQ(enterprise_reporting::kInvalidInputErrorMessage,
            RunFunctionAndReturnError(
                CreateChromeDesktopReportingFunction(kFakeDMToken),
                GenerateInvalidReport()));
}

TEST_F(EnterpriseReportingPrivateUploadChromeDesktopReportTest, UploadFailed) {
  ExtensionFunction* function =
      CreateChromeDesktopReportingFunction(kFakeDMToken);
  EXPECT_CALL(*client_, SetupRegistration(kFakeDMToken, kFakeClientId, _))
      .Times(1);
  EXPECT_CALL(*client_, UploadChromeDesktopReportProxy(_, _))
      .WillOnce(WithArgs<1>(policy::ScheduleStatusCallback(false)));
  ASSERT_EQ(enterprise_reporting::kUploadFailed,
            RunFunctionAndReturnError(function,
                                      GenerateArgs(kFakeMachineNameReport)));
  ::testing::Mock::VerifyAndClearExpectations(client_);
}

TEST_F(EnterpriseReportingPrivateUploadChromeDesktopReportTest,
       UploadSucceeded) {
  ExtensionFunction* function =
      CreateChromeDesktopReportingFunction(kFakeDMToken);
  EXPECT_CALL(*client_, SetupRegistration(kFakeDMToken, kFakeClientId, _))
      .Times(1);
  EXPECT_CALL(*client_, UploadChromeDesktopReportProxy(_, _))
      .WillOnce(WithArgs<1>(policy::ScheduleStatusCallback(true)));
  ASSERT_EQ(nullptr, RunFunctionAndReturnValue(
                         function, GenerateArgs(kFakeMachineNameReport)));
  ::testing::Mock::VerifyAndClearExpectations(client_);
}

// Test for API enterprise.reportingPrivate.getDeviceId
class EnterpriseReportingPrivateGetDeviceIdTest : public ExtensionApiUnittest {
 public:
  EnterpriseReportingPrivateGetDeviceIdTest() = default;

  void SetClientId(const std::string& client_id) {
    storage_.SetClientId(client_id);
  }

 private:
  policy::FakeBrowserDMTokenStorage storage_;

  DISALLOW_COPY_AND_ASSIGN(EnterpriseReportingPrivateGetDeviceIdTest);
};

TEST_F(EnterpriseReportingPrivateGetDeviceIdTest, GetDeviceId) {
  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetDeviceIdFunction>();
  SetClientId(kFakeClientId);
  std::unique_ptr<base::Value> id =
      RunFunctionAndReturnValue(function.get(), "[]");
  ASSERT_TRUE(id);
  ASSERT_TRUE(id->is_string());
  EXPECT_EQ(kFakeClientId, id->GetString());
}

TEST_F(EnterpriseReportingPrivateGetDeviceIdTest, DeviceIdNotExist) {
  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetDeviceIdFunction>();
  SetClientId("");
  ASSERT_EQ(enterprise_reporting::kDeviceIdNotFound,
            RunFunctionAndReturnError(function.get(), "[]"));
}

// Test for API enterprise.reportingPrivate.getDeviceId
class EnterpriseReportingPrivateDeviceDataFunctionsTest
    : public ExtensionApiUnittest {
 public:
  EnterpriseReportingPrivateDeviceDataFunctionsTest() = default;

  void SetUp() override {
    ExtensionApiUnittest::SetUp();
    ASSERT_TRUE(fake_appdata_dir_.CreateUniqueTempDir());
    OverrideEndpointVerificationDirForTesting(fake_appdata_dir_.GetPath());
  }

 private:
  base::ScopedTempDir fake_appdata_dir_;

  DISALLOW_COPY_AND_ASSIGN(EnterpriseReportingPrivateDeviceDataFunctionsTest);
};

TEST_F(EnterpriseReportingPrivateDeviceDataFunctionsTest, StoreDeviceData) {
  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateSetDeviceDataFunction>();
  std::unique_ptr<base::ListValue> values = std::make_unique<base::ListValue>();
  values->AppendString("a");
  values->Append(
      std::make_unique<base::Value>(base::Value::BlobStorage({1, 2, 3})));
  extension_function_test_utils::RunFunction(function.get(), std::move(values),
                                             browser(),
                                             extensions::api_test_utils::NONE);
  ASSERT_TRUE(function->GetResultList());
  EXPECT_EQ(0u, function->GetResultList()->GetSize());
  EXPECT_TRUE(function->GetError().empty());
}

TEST_F(EnterpriseReportingPrivateDeviceDataFunctionsTest, DeviceDataMissing) {
  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetDeviceDataFunction>();
  std::unique_ptr<base::ListValue> values = std::make_unique<base::ListValue>();
  values->AppendString("b");
  extension_function_test_utils::RunFunction(function.get(), std::move(values),
                                             browser(),
                                             extensions::api_test_utils::NONE);
  ASSERT_TRUE(function->GetResultList());
  EXPECT_EQ(1u, function->GetResultList()->GetSize());
  EXPECT_TRUE(function->GetError().empty());
}

TEST_F(EnterpriseReportingPrivateDeviceDataFunctionsTest, DeviceBadId) {
  auto set_function =
      base::MakeRefCounted<EnterpriseReportingPrivateSetDeviceDataFunction>();
  std::unique_ptr<base::ListValue> set_values =
      std::make_unique<base::ListValue>();
  set_values->AppendString("a/b");
  set_values->Append(
      std::make_unique<base::Value>(base::Value::BlobStorage({1, 2, 3})));
  extension_function_test_utils::RunFunction(set_function.get(),
                                             std::move(set_values), browser(),
                                             extensions::api_test_utils::NONE);
  ASSERT_TRUE(set_function->GetError().empty());

  // Try to read the directory as a file and should fail.
  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetDeviceDataFunction>();
  std::unique_ptr<base::ListValue> values = std::make_unique<base::ListValue>();
  values->AppendString("a");
  extension_function_test_utils::RunFunction(function.get(), std::move(values),
                                             browser(),
                                             extensions::api_test_utils::NONE);
  ASSERT_TRUE(function->GetResultList());
  EXPECT_EQ(0u, function->GetResultList()->GetSize());
  EXPECT_FALSE(function->GetError().empty());
}

TEST_F(EnterpriseReportingPrivateDeviceDataFunctionsTest, RetrieveDeviceData) {
  auto set_function =
      base::MakeRefCounted<EnterpriseReportingPrivateSetDeviceDataFunction>();
  std::unique_ptr<base::ListValue> set_values =
      std::make_unique<base::ListValue>();
  set_values->AppendString("c");
  set_values->Append(
      std::make_unique<base::Value>(base::Value::BlobStorage({1, 2, 3})));
  extension_function_test_utils::RunFunction(set_function.get(),
                                             std::move(set_values), browser(),
                                             extensions::api_test_utils::NONE);
  ASSERT_TRUE(set_function->GetError().empty());

  auto get_function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetDeviceDataFunction>();
  std::unique_ptr<base::ListValue> values = std::make_unique<base::ListValue>();
  values->AppendString("c");
  extension_function_test_utils::RunFunction(get_function.get(),
                                             std::move(values), browser(),
                                             extensions::api_test_utils::NONE);
  const base::Value* single_result = nullptr;
  ASSERT_TRUE(get_function->GetResultList());
  EXPECT_TRUE(get_function->GetResultList()->Get(0, &single_result));
  EXPECT_TRUE(get_function->GetError().empty());
  ASSERT_TRUE(single_result);
  ASSERT_TRUE(single_result->is_blob());
  EXPECT_EQ(base::Value::BlobStorage({1, 2, 3}), single_result->GetBlob());

  // Clear the data and check that it is gone.
  auto set_function2 =
      base::MakeRefCounted<EnterpriseReportingPrivateSetDeviceDataFunction>();
  std::unique_ptr<base::ListValue> reset_values =
      std::make_unique<base::ListValue>();
  reset_values->AppendString("c");
  extension_function_test_utils::RunFunction(set_function2.get(),
                                             std::move(reset_values), browser(),
                                             extensions::api_test_utils::NONE);
  ASSERT_TRUE(set_function2->GetError().empty());

  auto get_function2 =
      base::MakeRefCounted<EnterpriseReportingPrivateGetDeviceDataFunction>();
  std::unique_ptr<base::ListValue> values2 =
      std::make_unique<base::ListValue>();
  values2->AppendString("c");
  extension_function_test_utils::RunFunction(get_function2.get(),
                                             std::move(values2), browser(),
                                             extensions::api_test_utils::NONE);
  ASSERT_TRUE(get_function2->GetResultList());
  EXPECT_TRUE(get_function2->GetResultList()->Get(0, &single_result));
  EXPECT_TRUE(get_function2->GetError().empty());
  ASSERT_TRUE(single_result);
  ASSERT_TRUE(single_result->is_blob());
  EXPECT_EQ(base::Value::BlobStorage(), single_result->GetBlob());
}

// TODO(pastarmovj): Remove once implementation for the other platform exists.
#if defined(OS_WIN)

// Test for API enterprise.reportingPrivate.getDeviceId
class EnterpriseReportingPrivateGetPersistentSecretFunctionTest
    : public ExtensionApiUnittest {
 public:
  EnterpriseReportingPrivateGetPersistentSecretFunctionTest() = default;

  void SetUp() override {
    ExtensionApiUnittest::SetUp();
#if defined(OS_WIN)
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
#endif
  }

 private:
#if defined(OS_WIN)
  registry_util::RegistryOverrideManager registry_override_manager_;
#endif

  DISALLOW_COPY_AND_ASSIGN(
      EnterpriseReportingPrivateGetPersistentSecretFunctionTest);
};

TEST_F(EnterpriseReportingPrivateGetPersistentSecretFunctionTest, GetSecret) {
  auto function = base::MakeRefCounted<
      EnterpriseReportingPrivateGetPersistentSecretFunction>();
  std::unique_ptr<base::Value> result1 =
      RunFunctionAndReturnValue(function.get(), "[]");
  ASSERT_TRUE(result1);
  ASSERT_TRUE(result1->is_blob());
  auto generated_blob = result1->GetBlob();

  // Re=running should not change the secret.
  auto function2 = base::MakeRefCounted<
      EnterpriseReportingPrivateGetPersistentSecretFunction>();
  std::unique_ptr<base::Value> result2 =
      RunFunctionAndReturnValue(function2.get(), "[]");
  ASSERT_TRUE(result2);
  ASSERT_TRUE(result2->is_blob());
  ASSERT_EQ(generated_blob, result2->GetBlob());
}

#endif  // defined(OS_WIN)

using EnterpriseReportingPrivateGetDeviceInfoTest = ExtensionApiUnittest;

TEST_F(EnterpriseReportingPrivateGetDeviceInfoTest, GetDeviceInfo) {
  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetDeviceInfoFunction>();
  std::unique_ptr<base::Value> device_info_value =
      RunFunctionAndReturnValue(function.get(), "[]");
  ASSERT_TRUE(device_info_value.get());

  enterprise_reporting_private::DeviceInfo info;
  ASSERT_TRUE(enterprise_reporting_private::DeviceInfo::Populate(
      *device_info_value, &info));

#if defined(OS_MACOSX)
  EXPECT_EQ("macOS", info.os_name);
#elif defined(OS_WIN)
  EXPECT_EQ("windows", info.os_name);
#elif defined(OS_LINUX)
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  env->SetVar("XDG_CURRENT_DESKTOP", "XFCE");
  EXPECT_EQ("linux", info.os_name);
#else
  // Verify a stub implementation.
  EXPECT_EQ("stubOS", info.os_name);
  EXPECT_EQ("0.0.0.0", info.os_version);
  EXPECT_EQ("midnightshift", info.device_host_name);
  EXPECT_EQ("topshot", info.device_model);
  EXPECT_EQ("twirlchange", info.serial_number);
  EXPECT_EQ(enterprise_reporting_private::SETTING_VALUE_ENABLED,
            info.screen_lock_secured);
  EXPECT_EQ(enterprise_reporting_private::SETTING_VALUE_DISABLED,
            info.disk_encrypted);
#endif
}

}  // namespace extensions
