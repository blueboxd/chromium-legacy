// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <cstdlib>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/external_data_manager.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::Return;

namespace policy {

namespace {

// The name of the template example in policy_test_cases.json that does not need
// to be parsed.
const char kTemplateSampleTest[] = "-- Template --";

const char kCrosSettingsPrefix[] = "cros.";

std::string GetPolicyName(const std::string& policy_name_decorated) {
  const size_t offset = policy_name_decorated.find('.');
  if (offset != std::string::npos)
    return policy_name_decorated.substr(0, offset);
  return policy_name_decorated;
}

// Contains the details of a single test case verifying that the controlled
// setting indicators for a pref affected by a policy work correctly. This is
// part of the data loaded from chrome/test/data/policy/policy_test_cases.json.
class PrefIndicatorTest {
 public:
  explicit PrefIndicatorTest(const base::Value& indicator_test) {
    base::Optional<bool> readonly = indicator_test.FindBoolKey("readonly");
    const std::string* value = indicator_test.FindStringKey("value");
    const std::string* selector = indicator_test.FindStringKey("selector");
    const std::string* test_url = indicator_test.FindStringKey("test_url");
    const std::string* pref = indicator_test.FindStringKey("pref");
    const std::string* test_setup_js =
        indicator_test.FindStringKey("test_setup_js");

    readonly_ = readonly.value_or(false);
    value_ = value ? *value : std::string();
    test_url_ = test_url ? *test_url : std::string();
    test_setup_js_ = test_setup_js ? *test_setup_js : std::string();
    selector_ = selector ? *selector : std::string();
    pref_ = pref ? *pref : std::string();
  }

  ~PrefIndicatorTest() {}

  const std::string& value() const { return value_; }
  const std::string& test_url() const { return test_url_; }
  const std::string& test_setup_js() const { return test_setup_js_; }
  const std::string& selector() const { return selector_; }
  const std::string& pref() const { return pref_; }

  bool readonly() const { return readonly_; }

 private:
  bool readonly_;
  std::string value_;
  std::string test_url_;
  std::string test_setup_js_;
  std::string selector_;
  std::string pref_;

  DISALLOW_COPY_AND_ASSIGN(PrefIndicatorTest);
};

// Contains the testing details for a single pref affected by one or multiple
// policies. This is part of the data loaded from
// chrome/test/data/policy/policy_test_cases.json.
class PrefTestCase {
 public:
  explicit PrefTestCase(const std::string& name, const base::Value& settings) {
    const base::Value* value = settings.FindKey("value");
    const base::Value* indicator_test = settings.FindDictKey("indicator_test");
    is_local_state_ = settings.FindBoolKey("local_state").value_or(false);
    check_for_mandatory_ =
        settings.FindBoolKey("check_for_mandatory").value_or(true);
    check_for_recommended_ =
        settings.FindBoolKey("check_for_recommended").value_or(true);
    expect_default_ = settings.FindBoolKey("expect_default").value_or(false);

    pref_ = name;
    if (value)
      value_ = value->CreateDeepCopy();
    if (indicator_test) {
      pref_indicator_test_ =
          std::make_unique<PrefIndicatorTest>(*indicator_test);
    }
  }

  ~PrefTestCase() {}

  const std::string& pref() const { return pref_; }
  const base::Value* value() const { return value_.get(); }

  bool is_local_state() const { return is_local_state_; }

  bool check_for_mandatory() const { return check_for_mandatory_; }

  bool check_for_recommended() const { return check_for_recommended_; }

  bool expect_default() const { return expect_default_; }

  const PrefIndicatorTest* indicator_test_case() const {
    return pref_indicator_test_.get();
  }

 private:
  std::string pref_;
  std::unique_ptr<base::Value> value_;
  bool is_local_state_;
  bool check_for_mandatory_;
  bool check_for_recommended_;
  bool expect_default_;
  std::unique_ptr<PrefIndicatorTest> pref_indicator_test_;
  DISALLOW_COPY_AND_ASSIGN(PrefTestCase);
};

// Contains the testing details for a single pref affected by a policy. This is
// part of the data loaded from chrome/test/data/policy/policy_test_cases.json.
class PolicyPrefMappingTest {
 public:
  explicit PolicyPrefMappingTest(const base::Value& mapping) {
    const base::Value* policies = mapping.FindDictKey("policies");
    const base::Value* prefs = mapping.FindDictKey("prefs");
    if (policies)
      policies_ = policies->Clone();
    if (prefs) {
      for (const auto& pref_setting : prefs->DictItems())
        prefs_.push_back(std::make_unique<PrefTestCase>(pref_setting.first,
                                                        pref_setting.second));
    }
  }
  ~PolicyPrefMappingTest() {}

  const base::Value& policies() const { return policies_; }

  const std::vector<std::unique_ptr<PrefTestCase>>& prefs() const {
    return prefs_;
  }

 private:
  const std::string pref_;
  base::Value policies_;
  std::vector<std::unique_ptr<PrefTestCase>> prefs_;

  DISALLOW_COPY_AND_ASSIGN(PolicyPrefMappingTest);
};

// Contains the testing details for a single policy. This is part of the data
// loaded from chrome/test/data/policy/policy_test_cases.json.
class PolicyTestCase {
 public:
  PolicyTestCase(const std::string& name, const base::Value& test_case)
      : name_(name) {
    is_official_only_ = test_case.FindBoolKey("official_only").value_or(false);
    can_be_recommended_ =
        test_case.FindBoolKey("can_be_recommended").value_or(false);

    const base::Value* os_list = test_case.FindListKey("os");
    if (os_list) {
      for (const auto& os : os_list->GetList()) {
        if (os.is_string())
          supported_os_.push_back(os.GetString());
      }
    }

    const base::Value* test_policy = test_case.FindDictKey("test_policy");
    if (test_policy)
      test_policy_ = test_policy->CreateDeepCopy();

    const base::Value* policy_pref_mapping_tests =
        test_case.FindListKey("policy_pref_mapping_test");
    if (policy_pref_mapping_tests) {
      for (const auto& mapping : policy_pref_mapping_tests->GetList()) {
        if (mapping.is_dict()) {
          policy_pref_mapping_test_.push_back(
              std::make_unique<PolicyPrefMappingTest>(mapping));
        }
      }
    }
  }

  ~PolicyTestCase() {}

  const std::string& name() const { return name_; }

  bool is_official_only() const { return is_official_only_; }

  bool can_be_recommended() const { return can_be_recommended_; }

  bool IsOsSupported() const {
#if defined(OS_WIN)
    const std::string os("win");
#elif defined(OS_MACOSX)
    const std::string os("mac");
#elif defined(OS_CHROMEOS)
    const std::string os("chromeos");
#elif defined(OS_LINUX)
    const std::string os("linux");
#else
#error "Unknown platform"
#endif
    return base::Contains(supported_os_, os);
  }
  void AddSupportedOs(const std::string& os) { supported_os_.push_back(os); }

  bool IsSupported() const {
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
    if (is_official_only())
      return false;
#endif
    return IsOsSupported();
  }

  const base::Value* test_policy() const { return test_policy_.get(); }

  const std::vector<std::unique_ptr<PolicyPrefMappingTest>>&
  policy_pref_mapping_test() const {
    return policy_pref_mapping_test_;
  }

 private:
  std::string name_;
  bool is_official_only_;
  bool can_be_recommended_;
  std::vector<std::string> supported_os_;
  std::unique_ptr<base::Value> test_policy_;
  std::vector<std::unique_ptr<PolicyPrefMappingTest>> policy_pref_mapping_test_;

  DISALLOW_COPY_AND_ASSIGN(PolicyTestCase);
};

// Parses all policy test cases and makes them available in a map.
class PolicyTestCases {
 public:
  typedef std::vector<std::unique_ptr<PolicyTestCase>> PolicyTestCaseVector;
  typedef std::map<std::string, PolicyTestCaseVector> PolicyTestCaseMap;
  typedef PolicyTestCaseMap::const_iterator iterator;

  PolicyTestCases() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath path = ui_test_utils::GetTestFilePath(
        base::FilePath(FILE_PATH_LITERAL("policy")),
        base::FilePath(FILE_PATH_LITERAL("policy_test_cases.json")));
    std::string json;
    if (!base::ReadFileToString(path, &json)) {
      ADD_FAILURE();
      return;
    }
    int error_code = -1;
    std::string error_string;
    base::DictionaryValue* dict = nullptr;
    std::unique_ptr<base::Value> value =
        base::JSONReader::ReadAndReturnErrorDeprecated(
            json, base::JSON_PARSE_RFC, &error_code, &error_string);
    if (!value.get() || !value->GetAsDictionary(&dict)) {
      ADD_FAILURE() << "Error parsing policy_test_cases.json: " << error_string;
      return;
    }
    for (const auto& it : dict->DictItems()) {
      const std::string policy_name = GetPolicyName(it.first);
      if (policy_name == kTemplateSampleTest)
        continue;
      auto policy_test_case =
          std::make_unique<PolicyTestCase>(it.first, it.second);
      policy_test_cases_[policy_name].push_back(std::move(policy_test_case));
    }
  }

  ~PolicyTestCases() {}

  const PolicyTestCaseVector* Get(const std::string& name) const {
    const iterator it = policy_test_cases_.find(name);
    return it == end() ? nullptr : &it->second;
  }

  const PolicyTestCaseMap& map() const { return policy_test_cases_; }
  iterator begin() const { return policy_test_cases_.begin(); }
  iterator end() const { return policy_test_cases_.end(); }

 private:
  PolicyTestCaseMap policy_test_cases_;

  DISALLOW_COPY_AND_ASSIGN(PolicyTestCases);
};

}  // namespace

typedef InProcessBrowserTest PolicyPrefsTestCoverageTest;

IN_PROC_BROWSER_TEST_F(PolicyPrefsTestCoverageTest, AllPoliciesHaveATestCase) {
  // Verifies that all known policies have a test case in the JSON file.
  // This test fails when a policy is added to
  // components/policy/resources/policy_templates.json but a test case is not
  // added to chrome/test/data/policy/policy_test_cases.json.
  Schema chrome_schema = Schema::Wrap(GetChromeSchemaData());
  ASSERT_TRUE(chrome_schema.valid());

  PolicyTestCases policy_test_cases;
  for (Schema::Iterator it = chrome_schema.GetPropertiesIterator();
       !it.IsAtEnd(); it.Advance()) {
    auto policy = policy_test_cases.map().find(it.key());
    if (policy == policy_test_cases.map().end()) {
      ADD_FAILURE() << "Missing policy test case for: " << it.key();
      continue;
    }

    bool has_test_case_for_this_os = false;
    for (const auto& test_case : policy->second) {
      has_test_case_for_this_os |= test_case->IsSupported();
      if (has_test_case_for_this_os)
        break;
    }

    // This can only be a warning as many policies are not really testable
    // this way and only present as a single line in the file.
    // Although they could at least contain the "os" and "test_policy" fields.
    // See http://crbug.com/791125.
    LOG_IF(WARNING, !has_test_case_for_this_os)
        << "Policy " << policy->first
        << " is marked as supported on this OS in policy_templates.json but "
        << "have a test for this platform in policy_test_cases.json.";
  }
}

// Base class for tests that change policy.
class PolicyPrefsTest : public InProcessBrowserTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    EXPECT_CALL(provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));
    BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  void SetUpOnMainThread() override {
    search_test_utils::WaitForTemplateURLServiceToLoad(
        TemplateURLServiceFactory::GetForProfile(browser()->profile()));
  }

  void TearDownOnMainThread() override { ClearProviderPolicy(); }

  void ClearProviderPolicy() {
    provider_.UpdateChromePolicy(PolicyMap());
    base::RunLoop().RunUntilIdle();
  }

  void SetProviderPolicy(const base::Value& policies, PolicyLevel level) {
    PolicyMap policy_map;
    for (const auto& it : policies.DictItems()) {
      const PolicyDetails* policy_details = GetChromePolicyDetails(it.first);
      ASSERT_TRUE(policy_details);
      policy_map.Set(
          it.first, level, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
          it.second.CreateDeepCopy(),
          policy_details->max_external_data_size
              ? std::make_unique<ExternalDataFetcher>(nullptr, it.first)
              : nullptr);
    }
    provider_.UpdateChromePolicy(policy_map);
    base::RunLoop().RunUntilIdle();
  }

  MockConfigurationPolicyProvider provider_;
};

// Verifies that policies make their corresponding preferences become managed,
// and that the user can't override that setting.
IN_PROC_BROWSER_TEST_F(PolicyPrefsTest, PolicyToPrefsMapping) {
  Schema chrome_schema = Schema::Wrap(GetChromeSchemaData());
  ASSERT_TRUE(chrome_schema.valid());
  PrefService* local_state = g_browser_process->local_state();
  PrefService* user_prefs = browser()->profile()->GetPrefs();

  const PolicyTestCases test_cases;
  for (const auto& policy : test_cases) {
    for (const auto& test_case : policy.second) {
      const auto& pref_mappings = test_case->policy_pref_mapping_test();
      if (!chrome_schema.GetKnownProperty(policy.first).valid()) {
        // If the policy is supported on this platform according to the test it
        // should be known otherwise we signal this as a failure.
        // =====================================================================
        // !NOTE! If you see this assertion after changing Chrome's VERSION most
        // probably the mentioned policy was deprecated and deleted. Verify this
        // in policy_templates.json and remove the corresponding test entry
        // in policy_test_cases.json. Don't completely delete it from there just
        // replace it's definition with a single "note" value stating its
        // deprecation date (see other examples present in the file already).
        // =====================================================================
        EXPECT_FALSE(test_case->IsSupported())
            << "Policy " << policy.first
            << " is marked as supported on this OS but does not exist in the "
            << "Chrome policy schema.";
        continue;
      }

      if (!test_case->IsSupported() || pref_mappings.empty())
        continue;

      LOG(INFO) << "Testing policy: " << policy.first;

      for (const auto& pref_mapping : pref_mappings) {
        for (const auto& pref_case : pref_mapping->prefs()) {
          // Skip Chrome OS preferences that use a different backend and cannot
          // be retrieved through the prefs mechanism.
          if (base::StartsWith(pref_case->pref(), kCrosSettingsPrefix,
                               base::CompareCase::SENSITIVE))
            continue;

          // Skip preferences that should not be checked when the policy is set
          // to a mandatory value.
          if (!pref_case->check_for_mandatory())
            continue;

          PrefService* prefs =
              pref_case->is_local_state() ? local_state : user_prefs;
          // The preference must have been registered.
          const PrefService::Preference* pref =
              prefs->FindPreference(pref_case->pref().c_str());
          ASSERT_TRUE(pref)
              << "Pref " << pref_case->pref().c_str() << " not registered";

          // Verify that setting the policy overrides the pref.
          ClearProviderPolicy();
          prefs->ClearPref(pref_case->pref().c_str());
          EXPECT_TRUE(pref->IsDefaultValue());
          EXPECT_TRUE(pref->IsUserModifiable());
          EXPECT_FALSE(pref->IsUserControlled());
          EXPECT_FALSE(pref->IsManaged());

          ASSERT_NO_FATAL_FAILURE(SetProviderPolicy(pref_mapping->policies(),
                                                    POLICY_LEVEL_MANDATORY));
          if (pref_case->expect_default()) {
            EXPECT_TRUE(pref->IsDefaultValue());
            EXPECT_TRUE(pref->IsUserModifiable());
            EXPECT_FALSE(pref->IsUserControlled());
            EXPECT_FALSE(pref->IsManaged());
          } else {
            EXPECT_FALSE(pref->IsDefaultValue());
            EXPECT_FALSE(pref->IsUserModifiable());
            EXPECT_FALSE(pref->IsUserControlled());
            EXPECT_TRUE(pref->IsManaged());
          }
          if (pref_case->value())
            EXPECT_TRUE(pref->GetValue()->Equals(pref_case->value()));
        }
      }
    }
  }
}

// For WebUI integration tests, see cr_policy_indicator_tests.js and
// cr_policy_pref_indicator_tests.js.

}  // namespace policy
