// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <string>

#include "base/strings/utf_string_conversions.h"

#include "chrome/browser/ui/webui/management_ui_handler.h"
#include "chrome/test/base/testing_profile.h"

#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "ui/chromeos/devicetype_utils.h"
#endif  // defined(OS_CHROMEOS)

using testing::_;
using testing::Return;
using testing::ReturnRef;

struct ContextualManagementSourceUpdate {
  base::string16 extension_reporting_title;
  base::string16 subtitle;
#if defined(OS_CHROMEOS)
  base::string16 management_overview;
#else
  base::string16 browser_management_notice;
#endif  // defined(OS_CHROMEOS)
  bool managed;
};

class TestManagementUIHandler : public ManagementUIHandler {
 public:
  TestManagementUIHandler() = default;
  explicit TestManagementUIHandler(policy::PolicyService* policy_service)
      : policy_service_(policy_service) {}
  ~TestManagementUIHandler() override = default;

  void EnableCloudReportingExtension(bool enable) {
    cloud_reporting_extension_exists_ = enable;
  }

  base::Value GetContextualManagedDataForTesting(Profile* profile) {
    return GetContextualManagedData(profile);
  }

  base::Value GetExtensionReportingInfo() {
    base::Value report_sources(base::Value::Type::LIST);
    AddReportingInfo(&report_sources);
    return report_sources;
  }

  base::Value GetThreatProtectionInfo(Profile* profile) {
    return ManagementUIHandler::GetThreatProtectionInfo(profile);
  }

  policy::PolicyService* GetPolicyService() const override {
    return policy_service_;
  }

  const extensions::Extension* GetEnabledExtension(
      const std::string& extensionId) const override {
    if (cloud_reporting_extension_exists_)
      return extensions::ExtensionBuilder("dummy").SetID("id").Build().get();
    return nullptr;
  }

#if defined(OS_CHROMEOS)
  const std::string GetDeviceDomain() const override { return device_domain; }
  void SetDeviceDomain(const std::string& domain) { device_domain = domain; }
#endif  // defined(OS_CHROMEOS)

 private:
  bool cloud_reporting_extension_exists_ = false;
  policy::PolicyService* policy_service_ = nullptr;
  std::string device_domain = "devicedomain.com";
};

class ManagementUIHandlerTests : public testing::Test {
 public:
  ManagementUIHandlerTests()
      : handler_(&policy_service_),
        device_domain_(base::UTF8ToUTF16("devicedomain.com")) {
    ON_CALL(policy_service_, GetPolicies(_))
        .WillByDefault(ReturnRef(empty_policy_map_));
  }

  base::string16 device_domain() { return device_domain_; }
  void EnablePolicy(const char* policy_key, policy::PolicyMap& policies) {
    policies.Set(policy_key, policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                 std::make_unique<base::Value>(true), nullptr);
  }
  void SetPolicyValue(const char* policy_key,
                      policy::PolicyMap& policies,
                      int value) {
    policies.Set(policy_key, policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                 std::make_unique<base::Value>(value), nullptr);
  }
  void SetPolicyValue(const char* policy_key,
                      policy::PolicyMap& policies,
                      bool value) {
    policies.Set(policy_key, policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                 std::make_unique<base::Value>(value), nullptr);
  }

  base::string16 ExtractPathFromDict(const base::Value& data,
                                     const std::string path) {
    const std::string* buf = data.FindStringPath(path);
    if (!buf)
      return base::string16();
    return base::UTF8ToUTF16(*buf);
  }

  void ExtractContextualSourceUpdate(const base::Value& data) {
    extracted_.extension_reporting_title =
        ExtractPathFromDict(data, "extensionReportingTitle");
    extracted_.subtitle = ExtractPathFromDict(data, "pageSubtitle");
#if defined(OS_CHROMEOS)
    extracted_.management_overview = ExtractPathFromDict(data, "overview");
#else
    extracted_.browser_management_notice =
        ExtractPathFromDict(data, "browserManagementNotice");
#endif  // defined(OS_CHROMEOS)
    base::Optional<bool> managed = data.FindBoolPath("managed");
    extracted_.managed = managed.has_value() && managed.value();
  }

  void PrepareProfileAndHandler() {
    PrepareProfileAndHandler(std::string(), false, true, false,
                             "devicedomain.com");
  }

  void PrepareProfileAndHandler(const std::string& profile_name,
                                bool override_policy_connector_is_managed,
                                bool use_account,
                                bool use_device) {
    PrepareProfileAndHandler(profile_name, override_policy_connector_is_managed,
                             use_account, use_device, "devicedomain.com");
  }

  void PrepareProfileAndHandler(const std::string& profile_name,
                                bool override_policy_connector_is_managed,
                                bool use_account,
                                bool use_device,
                                const std::string& device_domain) {
    TestingProfile::Builder builder;
    builder.SetProfileName(profile_name);
    if (override_policy_connector_is_managed) {
      builder.OverridePolicyConnectorIsManagedForTesting(true);
    }
    profile_ = builder.Build();
    handler_.SetAccountManagedForTesting(use_account);
    handler_.SetDeviceManagedForTesting(use_device);
#if defined(OS_CHROMEOS)
    handler_.SetDeviceDomain(device_domain);
#endif
    base::Value data =
        handler_.GetContextualManagedDataForTesting(profile_.get());
    ExtractContextualSourceUpdate(data);
  }

  bool GetManaged() const { return extracted_.managed; }

#if defined(OS_CHROMEOS)
  base::string16 GetManagementOverview() const {
    return extracted_.management_overview;
  }
#else
  base::string16 GetBrowserManagementNotice() const {
    return extracted_.browser_management_notice;
  }
#endif

  base::string16 GetExtensionReportingTitle() const {
    return extracted_.extension_reporting_title;
  }

  base::string16 GetPageSubtitle() const { return extracted_.subtitle; }

 protected:
  TestManagementUIHandler handler_;
  content::BrowserTaskEnvironment task_environment_;
  policy::MockPolicyService policy_service_;
  policy::PolicyMap empty_policy_map_;
  base::string16 device_domain_;
  ContextualManagementSourceUpdate extracted_;
  std::unique_ptr<TestingProfile> profile_;
};

void ExpectMessagesToBeEQ(base::Value::ConstListView infolist,
                          const std::set<std::string>& expected_messages) {
  ASSERT_EQ(infolist.size(), expected_messages.size());
  std::set<std::string> tmp_expected(expected_messages);
  for (const base::Value& info : infolist) {
    const std::string* message_id = info.FindStringKey("messageId");
    if (message_id) {
      EXPECT_EQ(1u, tmp_expected.erase(*message_id));
    }
  }
  EXPECT_TRUE(tmp_expected.empty());
}

#if !defined(OS_CHROMEOS)
TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateUnmanagedNoDomain) {
  PrepareProfileAndHandler(
      /* profile_name= */ "",
      /* override_policy_connector_is_managed= */ false,
      /* use_account= */ false,
      /* use_device= */ false);

  EXPECT_EQ(GetExtensionReportingTitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(GetBrowserManagementNotice(),
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_NOT_MANAGED_NOTICE,
                base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl)));
  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE));
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedNoDomain) {
  PrepareProfileAndHandler();

  EXPECT_EQ(GetExtensionReportingTitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(GetBrowserManagementNotice(),
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_BROWSER_NOTICE,
                base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl)));
  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_SUBTITLE));
  EXPECT_TRUE(GetManaged());
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedConsumerDomain) {
  PrepareProfileAndHandler(
      /* profile_name= */ "managed@gmail.com",
      /* override_policy_connector_is_managed= */ true,
      /* use_account= */ true, /* use_device= */ false);

  EXPECT_EQ(GetExtensionReportingTitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(GetBrowserManagementNotice(),
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_BROWSER_NOTICE,
                base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl)));
  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_SUBTITLE));
  EXPECT_TRUE(GetManaged());
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateUnmanagedKnownDomain) {
  const std::string domain = "manager.com";
  PrepareProfileAndHandler(
      /* profile_name= */ "managed@" + domain,
      /* override_policy_connector_is_managed= */ true,
      /* use_account= */ false, /* use_device= */ false);

  EXPECT_EQ(GetExtensionReportingTitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED_BY,
                                       base::UTF8ToUTF16(domain)));
  EXPECT_EQ(GetBrowserManagementNotice(),
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_NOT_MANAGED_NOTICE,
                base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl)));
  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE));
  EXPECT_FALSE(GetManaged());
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateUnmanagedCustomerDomain) {
  PrepareProfileAndHandler(
      /* profile_name= */ "managed@googlemail.com",
      /* override_policy_connector_is_managed= */ false,
      /* use_account= */ false, /* use_device= */ false);
  EXPECT_EQ(GetExtensionReportingTitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(GetBrowserManagementNotice(),
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_NOT_MANAGED_NOTICE,
                base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl)));
  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE));
  EXPECT_FALSE(GetManaged());
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedKnownDomain) {
  const std::string domain = "gmail.com.manager.com.gmail.com";
  PrepareProfileAndHandler(
      /* profile_name= */ "managed@" + domain,
      /* override_policy_connector_is_managed= */ true,
      /* use_account_for_testing= */ true, /* use_device= */ false);

  EXPECT_EQ(GetExtensionReportingTitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED_BY,
                                       base::UTF8ToUTF16(domain)));
  EXPECT_EQ(GetBrowserManagementNotice(),
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_BROWSER_NOTICE,
                base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl)));
  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                       base::UTF8ToUTF16(domain)));
  EXPECT_TRUE(GetManaged());
}

#endif  // !defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedAccountKnownDomain) {
  const std::string domain = "manager.com";
  PrepareProfileAndHandler(
      /* profile_name= */ "managed@" + domain,
      /* override_policy_connector_is_managed= */ true,
      /* use_account= */ true, /* use_device= */ false,
      /* device_name= */ "");
  const auto device_type = ui::GetChromeOSDeviceTypeResourceId();

  EXPECT_EQ(GetExtensionReportingTitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED_BY,
                                       base::UTF8ToUTF16(domain)));
  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                       l10n_util::GetStringUTF16(device_type),
                                       base::UTF8ToUTF16(domain)));
  EXPECT_EQ(GetManagementOverview(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_ACCOUNT_MANAGED_BY,
                                       base::UTF8ToUTF16(domain)));
  EXPECT_TRUE(GetManaged());
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedAccountUnknownDomain) {
  PrepareProfileAndHandler(
      /* profile_name= */ "",
      /* override_policy_connector_is_managed= */ false,
      /* use_account= */ true, /* use_device= */ false,
      /* device_name= */ "");
  const auto device_type = ui::GetChromeOSDeviceTypeResourceId();

  EXPECT_EQ(GetExtensionReportingTitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED,
                                       l10n_util::GetStringUTF16(device_type)));
  EXPECT_EQ(GetManagementOverview(), base::string16());
  EXPECT_TRUE(GetManaged());
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedDevice) {
  PrepareProfileAndHandler(
      /* profile_name= */ "managed@manager.com",
      /* override_policy_connector_is_managed= */ false,
      /* use_account= */ false, /* use_device= */ true);
  const auto device_type = ui::GetChromeOSDeviceTypeResourceId();

  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                       l10n_util::GetStringUTF16(device_type),
                                       device_domain()));
  EXPECT_EQ(GetExtensionReportingTitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED_BY,
                                       device_domain()));
  EXPECT_EQ(GetManagementOverview(), base::string16());
  EXPECT_TRUE(GetManaged());
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedDeviceAndAccount) {
  PrepareProfileAndHandler(
      /* profile_name= */ "managed@devicedomain.com",
      /* override_policy_connector_is_managed= */ false,
      /* use_account= */ true, /* use_device= */ true);
  const auto device_type = ui::GetChromeOSDeviceTypeResourceId();

  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                       l10n_util::GetStringUTF16(device_type),
                                       device_domain()));
  EXPECT_EQ(GetExtensionReportingTitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED_BY,
                                       device_domain()));
  EXPECT_EQ(GetManagementOverview(),
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_DEVICE_AND_ACCOUNT_MANAGED_BY, device_domain()));
  EXPECT_TRUE(GetManaged());
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedDeviceAndAccountMultipleDomains) {
  const std::string domain = "manager.com";
  PrepareProfileAndHandler(
      /* profile_name= */ "managed@" + domain,
      /* override_policy_connector_is_managed= */ true,
      /* use_account= */ true, /* use_device= */ true);
  const auto device_type = ui::GetChromeOSDeviceTypeResourceId();

  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                       l10n_util::GetStringUTF16(device_type),
                                       device_domain()));
  EXPECT_EQ(GetExtensionReportingTitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED_BY,
                                       device_domain()));
  EXPECT_EQ(GetManagementOverview(),
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_DEVICE_MANAGED_BY_ACCOUNT_MANAGED_BY,
                device_domain(), base::UTF8ToUTF16(domain)));
  EXPECT_TRUE(GetManaged());
}

TEST_F(ManagementUIHandlerTests, ManagementContextualSourceUpdateUnmanaged) {
  PrepareProfileAndHandler(
      /* profile_name= */ "",
      /* override_policy_connector_is_managed= */ false,
      /* use_account= */ false, /* use_device= */ false,
      /* device_domain= */ "");
  const auto device_type = ui::GetChromeOSDeviceTypeResourceId();

  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE,
                                       l10n_util::GetStringUTF16(device_type)));
  EXPECT_EQ(GetExtensionReportingTitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(GetManagementOverview(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_DEVICE_NOT_MANAGED));
  EXPECT_FALSE(GetManaged());
}
#endif

TEST_F(ManagementUIHandlerTests, ExtensionReportingInfoNoPolicySetNoMessage) {
  handler_.EnableCloudReportingExtension(false);
  auto reporting_info = handler_.GetExtensionReportingInfo();
  EXPECT_EQ(reporting_info.GetList().size(), 0u);
}

TEST_F(ManagementUIHandlerTests,
       ExtensionReportingInfoCloudExtensionAddsDefaultPolicies) {
  handler_.EnableCloudReportingExtension(true);

  const std::set<std::string> expected_messages = {
      kManagementExtensionReportMachineName, kManagementExtensionReportUsername,
      kManagementExtensionReportVersion,
      kManagementExtensionReportExtensionsPlugin,
      kManagementExtensionReportSafeBrowsingWarnings};

  ExpectMessagesToBeEQ(handler_.GetExtensionReportingInfo().GetList(),
                       expected_messages);
}

TEST_F(ManagementUIHandlerTests, CloudReportingPolicy) {
  handler_.EnableCloudReportingExtension(false);

  policy::PolicyMap chrome_policies;
  const policy::PolicyNamespace chrome_policies_namespace =
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string());
  EXPECT_CALL(policy_service_, GetPolicies(_))
      .WillRepeatedly(ReturnRef(chrome_policies));
  SetPolicyValue(policy::key::kCloudReportingEnabled, chrome_policies, true);

  std::set<std::string> expected_messages = {
      kManagementExtensionReportMachineName, kManagementExtensionReportUsername,
      kManagementExtensionReportVersion,
      kManagementExtensionReportExtensionsPlugin};

  ExpectMessagesToBeEQ(handler_.GetExtensionReportingInfo().GetList(),
                       expected_messages);
}

TEST_F(ManagementUIHandlerTests, ExtensionReportingInfoPoliciesMerge) {
  policy::PolicyMap on_prem_reporting_extension_beta_policies;
  policy::PolicyMap on_prem_reporting_extension_stable_policies;

  EnablePolicy(kPolicyKeyReportUserIdData,
               on_prem_reporting_extension_beta_policies);
  EnablePolicy(kManagementExtensionReportVersion,
               on_prem_reporting_extension_beta_policies);
  EnablePolicy(kPolicyKeyReportUserIdData,
               on_prem_reporting_extension_beta_policies);
  EnablePolicy(kPolicyKeyReportPolicyData,
               on_prem_reporting_extension_stable_policies);

  EnablePolicy(kPolicyKeyReportMachineIdData,
               on_prem_reporting_extension_stable_policies);
  EnablePolicy(kPolicyKeyReportSafeBrowsingData,
               on_prem_reporting_extension_stable_policies);
  EnablePolicy(kPolicyKeyReportSystemTelemetryData,
               on_prem_reporting_extension_stable_policies);
  EnablePolicy(kPolicyKeyReportUserBrowsingData,
               on_prem_reporting_extension_stable_policies);

  const policy::PolicyNamespace
      on_prem_reporting_extension_stable_policy_namespace =
          policy::PolicyNamespace(policy::POLICY_DOMAIN_EXTENSIONS,
                                  kOnPremReportingExtensionStableId);
  const policy::PolicyNamespace
      on_prem_reporting_extension_beta_policy_namespace =
          policy::PolicyNamespace(policy::POLICY_DOMAIN_EXTENSIONS,
                                  kOnPremReportingExtensionBetaId);

  EXPECT_CALL(policy_service_,
              GetPolicies(on_prem_reporting_extension_stable_policy_namespace))
      .WillOnce(ReturnRef(on_prem_reporting_extension_stable_policies));

  EXPECT_CALL(policy_service_,
              GetPolicies(on_prem_reporting_extension_beta_policy_namespace))
      .WillOnce(ReturnRef(on_prem_reporting_extension_beta_policies));
  policy::PolicyMap empty_policy_map;
  EXPECT_CALL(policy_service_,
              GetPolicies(policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                                                  std::string())))
      .WillOnce(ReturnRef(empty_policy_map));

  handler_.EnableCloudReportingExtension(true);

  std::set<std::string> expected_messages = {
      kManagementExtensionReportMachineNameAddress,
      kManagementExtensionReportUsername,
      kManagementExtensionReportVersion,
      kManagementExtensionReportExtensionsPlugin,
      kManagementExtensionReportSafeBrowsingWarnings,
      kManagementExtensionReportUserBrowsingData,
      kManagementExtensionReportPerfCrash};

  ExpectMessagesToBeEQ(handler_.GetExtensionReportingInfo().GetList(),
                       expected_messages);
}

TEST_F(ManagementUIHandlerTests, ThreatReportingInfo) {
  policy::PolicyMap chrome_policies;
  const policy::PolicyNamespace chrome_policies_namespace =
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string());

  TestingProfile::Builder builder_no_domain;
  auto profile_no_domain = builder_no_domain.Build();

  TestingProfile::Builder builder_known_domain;
  builder_known_domain.SetProfileName("managed@manager.com");
  auto profile_known_domain = builder_known_domain.Build();

#if defined(OS_CHROMEOS)
  handler_.SetDeviceDomain("");
#endif  // !defined(OS_CHROMEOS)

  EXPECT_CALL(policy_service_, GetPolicies(chrome_policies_namespace))
      .WillRepeatedly(ReturnRef(chrome_policies));

  base::DictionaryValue* threat_protection_info = nullptr;

  // When no policies are set, nothing to report.
  auto info = handler_.GetThreatProtectionInfo(profile_no_domain.get());
  info.GetAsDictionary(&threat_protection_info);
  EXPECT_TRUE(threat_protection_info->FindListKey("info")->GetList().empty());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_THREAT_PROTECTION_DESCRIPTION),
      base::UTF8ToUTF16(*threat_protection_info->FindStringKey("description")));

  // When policies are set to uninteresting values, nothing to report.
  SetPolicyValue(policy::key::kCheckContentCompliance, chrome_policies, 0);
  SetPolicyValue(policy::key::kSendFilesForMalwareCheck, chrome_policies, 0);
  SetPolicyValue(policy::key::kUnsafeEventsReportingEnabled, chrome_policies,
                 false);
  info = handler_.GetThreatProtectionInfo(profile_known_domain.get());
  info.GetAsDictionary(&threat_protection_info);
  EXPECT_TRUE(threat_protection_info->FindListKey("info")->GetList().empty());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_THREAT_PROTECTION_DESCRIPTION),
      base::UTF8ToUTF16(*threat_protection_info->FindStringKey("description")));

  // When policies are set to values that enable the feature, report it.
  SetPolicyValue(policy::key::kCheckContentCompliance, chrome_policies, 1);
  SetPolicyValue(policy::key::kSendFilesForMalwareCheck, chrome_policies, 2);
  SetPolicyValue(policy::key::kUnsafeEventsReportingEnabled, chrome_policies,
                 true);
  info = handler_.GetThreatProtectionInfo(profile_no_domain.get());
  info.GetAsDictionary(&threat_protection_info);
  EXPECT_EQ(3u, threat_protection_info->FindListKey("info")->GetList().size());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_THREAT_PROTECTION_DESCRIPTION),
      base::UTF8ToUTF16(*threat_protection_info->FindStringKey("description")));

  base::Value expected_info(base::Value::Type::LIST);
  {
    base::Value value(base::Value::Type::DICTIONARY);
    value.SetStringKey("title", kManagementDataLossPreventionName);
    value.SetStringKey("permission", kManagementDataLossPreventionPermissions);
    expected_info.Append(std::move(value));
  }
  {
    base::Value value(base::Value::Type::DICTIONARY);
    value.SetStringKey("title", kManagementMalwareScanningName);
    value.SetStringKey("permission", kManagementMalwareScanningPermissions);
    expected_info.Append(std::move(value));
  }
  {
    base::Value value(base::Value::Type::DICTIONARY);
    value.SetStringKey("title", kManagementEnterpriseReportingName);
    value.SetStringKey("permission", kManagementEnterpriseReportingPermissions);
    expected_info.Append(std::move(value));
  }

  EXPECT_EQ(expected_info, *threat_protection_info->FindListKey("info"));
}
