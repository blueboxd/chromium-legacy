// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/extensions/api/declarative_net_request/dnr_test_base.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/parse_info.h"
#include "extensions/browser/api/declarative_net_request/test_utils.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/file_util.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/url_pattern.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace declarative_net_request {
namespace {

constexpr char kJSONRulesFilename[] = "rules_file.json";

constexpr char kLargeRegexFilter[] = ".{512}x";

namespace dnr_api = extensions::api::declarative_net_request;

std::string GetParseError(ParseResult result, int rule_id) {
  ParseInfo info;
  info.SetError(result, &rule_id);
  return info.error();
}

std::string GetErrorWithFilename(
    const std::string& error,
    const std::string& filename = kJSONRulesFilename) {
  return base::StringPrintf("%s: %s", filename.c_str(), error.c_str());
}

InstallWarning GetLargeRegexWarning(
    int rule_id,
    const std::string& filename = kJSONRulesFilename) {
  return InstallWarning(ErrorUtils::FormatErrorMessage(
                            GetErrorWithFilename(kErrorRegexTooLarge, filename),
                            base::NumberToString(rule_id), kRegexFilterKey),
                        manifest_keys::kDeclarativeNetRequestKey,
                        manifest_keys::kDeclarativeRuleResourcesKey);
}

// Base test fixture to test indexing of rulesets.
class RuleIndexingTestBase : public DNRTestBase {
 public:
  RuleIndexingTestBase() = default;

  // DNRTestBase override.
  void SetUp() override {
    DNRTestBase::SetUp();
    loader_ = CreateExtensionLoader();
    extension_dir_ =
        temp_dir().GetPath().Append(FILE_PATH_LITERAL("test_extension"));

    // Create extension directory.
    ASSERT_TRUE(base::CreateDirectory(extension_dir_));
  }

 protected:
  // Loads the extension and verifies the indexed ruleset location and histogram
  // counts.
  void LoadAndExpectSuccess(size_t expected_indexed_rules_count) {
    base::HistogramTester tester;
    WriteExtensionData();

    loader_->set_should_fail(false);

    // Clear all load errors before loading the extension.
    error_reporter()->ClearErrors();

    extension_ = loader_->LoadExtension(extension_dir_);
    ASSERT_TRUE(extension_.get());

    EXPECT_TRUE(
        AreAllIndexedStaticRulesetsValid(*extension_, browser_context()));

    // Ensure no load errors were reported.
    EXPECT_TRUE(error_reporter()->GetErrors()->empty());

    // The histograms below are not logged for unpacked extensions.
    if (GetParam() == ExtensionLoadType::PACKED) {
      tester.ExpectTotalCount(kIndexAndPersistRulesTimeHistogram,
                              1 /* count */);
      tester.ExpectBucketCount(kManifestRulesCountHistogram,
                               expected_indexed_rules_count, 1 /* count */);
    }
  }

  void LoadAndExpectError(const std::string& expected_error,
                          const std::string& filename) {
    // The error should be prepended with the JSON filename.
    std::string error_with_filename =
        GetErrorWithFilename(expected_error, filename);

    base::HistogramTester tester;
    WriteExtensionData();

    loader_->set_should_fail(true);

    // Clear all load errors before loading the extension.
    error_reporter()->ClearErrors();

    extension_ = loader_->LoadExtension(extension_dir_);
    EXPECT_FALSE(extension_.get());

    // Verify the error. Only verify if the |expected_error| is a substring of
    // the actual error, since some string may be prepended/appended while
    // creating the actual error.
    const std::vector<base::string16>* errors = error_reporter()->GetErrors();
    ASSERT_EQ(1u, errors->size());
    EXPECT_NE(base::string16::npos,
              errors->at(0).find(base::UTF8ToUTF16(error_with_filename)))
        << "expected: " << error_with_filename << " actual: " << errors->at(0);

    tester.ExpectTotalCount(kIndexAndPersistRulesTimeHistogram, 0u);
    tester.ExpectTotalCount(kManifestRulesCountHistogram, 0u);
  }

  ChromeTestExtensionLoader* extension_loader() { return loader_.get(); }

  const Extension* extension() const { return extension_.get(); }

  const base::FilePath& extension_dir() const { return extension_dir_; }

  LoadErrorReporter* error_reporter() {
    return LoadErrorReporter::GetInstance();
  }

 private:
  // Derived classes must override this to persist the extension to disk.
  virtual void WriteExtensionData() = 0;

  base::FilePath extension_dir_;
  std::unique_ptr<ChromeTestExtensionLoader> loader_;
  scoped_refptr<const Extension> extension_;
};

// Fixture testing that declarative rules corresponding to the Declarative Net
// Request API are correctly indexed, for both packed and unpacked extensions.
// This only tests a single ruleset.
class SingleRulesetIndexingTest : public RuleIndexingTestBase {
 public:
  SingleRulesetIndexingTest() = default;

 protected:
  void AddRule(const TestRule& rule) { rules_list_.push_back(rule); }

  // This takes precedence over the AddRule method.
  void SetRules(std::unique_ptr<base::Value> rules) {
    rules_value_ = std::move(rules);
  }

  void set_persist_invalid_json_file() { persist_invalid_json_file_ = true; }

  void set_persist_initial_indexed_ruleset() {
    persist_initial_indexed_ruleset_ = true;
  }

  void LoadAndExpectError(const std::string& expected_error) {
    RuleIndexingTestBase::LoadAndExpectError(expected_error,
                                             kJSONRulesFilename);
  }

 private:
  // RuleIndexingTestBase override:
  void WriteExtensionData() override {
    if (!rules_value_)
      rules_value_ = ToListValue(rules_list_);

    TestRulesetInfo info = {kJSONRulesFilename, std::move(*rules_value_)};
    WriteManifestAndRuleset(extension_dir(), info, {} /* hosts */);

    // Overwrite the JSON rules file with some invalid json.
    if (persist_invalid_json_file_) {
      std::string data = "invalid json";
      ASSERT_EQ(static_cast<int>(data.size()),
                base::WriteFile(extension_dir().AppendASCII(kJSONRulesFilename),
                                data.c_str(), data.size()));
    }

    if (persist_initial_indexed_ruleset_) {
      std::string data = "user ruleset";
      base::FilePath ruleset_path = extension_dir().Append(
          file_util::GetIndexedRulesetRelativePath(kMinValidStaticRulesetID));
      ASSERT_TRUE(base::CreateDirectory(ruleset_path.DirName()));
      ASSERT_EQ(static_cast<int>(data.size()),
                base::WriteFile(ruleset_path, data.c_str(), data.size()));
    }
  }

  std::vector<TestRule> rules_list_;
  std::unique_ptr<base::Value> rules_value_;
  bool persist_invalid_json_file_ = false;
  bool persist_initial_indexed_ruleset_ = false;
};

TEST_P(SingleRulesetIndexingTest, DuplicateResourceTypes) {
  TestRule rule = CreateGenericRule();
  rule.condition->resource_types =
      std::vector<std::string>({"image", "stylesheet"});
  rule.condition->excluded_resource_types = std::vector<std::string>({"image"});
  AddRule(rule);
  LoadAndExpectError(
      GetParseError(ParseResult::ERROR_RESOURCE_TYPE_DUPLICATED, *rule.id));
}

TEST_P(SingleRulesetIndexingTest, EmptyRedirectRulePriority) {
  TestRule rule = CreateGenericRule();
  rule.action->type = std::string("redirect");
  rule.action->redirect.emplace();
  rule.action->redirect->url = std::string("https://google.com");
  rule.priority.reset();
  AddRule(rule);
  LoadAndExpectError(
      GetParseError(ParseResult::ERROR_EMPTY_RULE_PRIORITY, *rule.id));
}

TEST_P(SingleRulesetIndexingTest, EmptyRedirectRuleUrl) {
  TestRule rule = CreateGenericRule();
  rule.id = kMinValidID;
  AddRule(rule);

  rule.id = kMinValidID + 1;
  rule.action->type = std::string("redirect");
  rule.priority = kMinValidPriority;
  AddRule(rule);

  LoadAndExpectError(
      GetParseError(ParseResult::ERROR_INVALID_REDIRECT, *rule.id));
}

TEST_P(SingleRulesetIndexingTest, InvalidRuleID) {
  TestRule rule = CreateGenericRule();
  rule.id = kMinValidID - 1;
  AddRule(rule);
  LoadAndExpectError(
      GetParseError(ParseResult::ERROR_INVALID_RULE_ID, *rule.id));
}

TEST_P(SingleRulesetIndexingTest, InvalidRedirectRulePriority) {
  TestRule rule = CreateGenericRule();
  rule.action->type = std::string("redirect");
  rule.action->redirect.emplace();
  rule.action->redirect->url = std::string("https://google.com");
  rule.priority = kMinValidPriority - 1;
  AddRule(rule);
  LoadAndExpectError(
      GetParseError(ParseResult::ERROR_INVALID_RULE_PRIORITY, *rule.id));
}

TEST_P(SingleRulesetIndexingTest, NoApplicableResourceTypes) {
  TestRule rule = CreateGenericRule();
  rule.condition->excluded_resource_types = std::vector<std::string>(
      {"main_frame", "sub_frame", "stylesheet", "script", "image", "font",
       "object", "xmlhttprequest", "ping", "csp_report", "media", "websocket",
       "other"});
  AddRule(rule);
  LoadAndExpectError(
      GetParseError(ParseResult::ERROR_NO_APPLICABLE_RESOURCE_TYPES, *rule.id));
}

TEST_P(SingleRulesetIndexingTest, EmptyDomainsList) {
  TestRule rule = CreateGenericRule();
  rule.condition->domains = std::vector<std::string>();
  AddRule(rule);
  LoadAndExpectError(
      GetParseError(ParseResult::ERROR_EMPTY_DOMAINS_LIST, *rule.id));
}

TEST_P(SingleRulesetIndexingTest, EmptyResourceTypeList) {
  TestRule rule = CreateGenericRule();
  rule.condition->resource_types = std::vector<std::string>();
  AddRule(rule);
  LoadAndExpectError(
      GetParseError(ParseResult::ERROR_EMPTY_RESOURCE_TYPES_LIST, *rule.id));
}

TEST_P(SingleRulesetIndexingTest, EmptyURLFilter) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string();
  AddRule(rule);
  LoadAndExpectError(
      GetParseError(ParseResult::ERROR_EMPTY_URL_FILTER, *rule.id));
}

TEST_P(SingleRulesetIndexingTest, InvalidRedirectURL) {
  TestRule rule = CreateGenericRule();
  rule.action->type = std::string("redirect");
  rule.action->redirect.emplace();
  rule.action->redirect->url = std::string("google");
  rule.priority = kMinValidPriority;
  AddRule(rule);
  LoadAndExpectError(
      GetParseError(ParseResult::ERROR_INVALID_REDIRECT_URL, *rule.id));
}

TEST_P(SingleRulesetIndexingTest, ListNotPassed) {
  SetRules(std::make_unique<base::DictionaryValue>());
  LoadAndExpectError(kErrorListNotPassed);
}

TEST_P(SingleRulesetIndexingTest, DuplicateIDS) {
  TestRule rule = CreateGenericRule();
  AddRule(rule);
  AddRule(rule);
  LoadAndExpectError(GetParseError(ParseResult::ERROR_DUPLICATE_IDS, *rule.id));
}

// Ensure that we limit the number of parse failure warnings shown.
TEST_P(SingleRulesetIndexingTest, TooManyParseFailures) {
  const size_t kNumInvalidRules = 10;
  const size_t kNumValidRules = 6;
  const size_t kMaxUnparsedRulesWarnings = 5;

  size_t rule_id = kMinValidID;
  for (size_t i = 0; i < kNumInvalidRules; i++) {
    TestRule rule = CreateGenericRule();
    rule.id = rule_id++;
    rule.action->type = std::string("invalid_action_type");
    AddRule(rule);
  }

  for (size_t i = 0; i < kNumValidRules; i++) {
    TestRule rule = CreateGenericRule();
    rule.id = rule_id++;
    AddRule(rule);
  }

  extension_loader()->set_ignore_manifest_warnings(true);
  LoadAndExpectSuccess(kNumValidRules);

  // TODO(crbug.com/879355): CrxInstaller reloads the extension after moving it,
  // which causes it to lose the install warning. This should be fixed.
  if (GetParam() != ExtensionLoadType::PACKED) {
    const std::vector<InstallWarning>& expected_warnings =
        extension()->install_warnings();
    ASSERT_EQ(1u + kMaxUnparsedRulesWarnings, expected_warnings.size());

    InstallWarning warning("");
    warning.key = manifest_keys::kDeclarativeNetRequestKey;
    warning.specific = manifest_keys::kDeclarativeRuleResourcesKey;

    // The initial warnings should correspond to the first
    // |kMaxUnparsedRulesWarnings| rules, which couldn't be parsed.
    for (size_t i = 0; i < kMaxUnparsedRulesWarnings; i++) {
      EXPECT_EQ(expected_warnings[i].key, warning.key);
      EXPECT_EQ(expected_warnings[i].specific, warning.specific);
      EXPECT_THAT(expected_warnings[i].message,
                  ::testing::HasSubstr("Parse error"));
    }

    warning.message = ErrorUtils::FormatErrorMessage(
        GetErrorWithFilename(kTooManyParseFailuresWarning),
        std::to_string(kMaxUnparsedRulesWarnings));
    EXPECT_EQ(warning, expected_warnings[kMaxUnparsedRulesWarnings]);
  }
}

// Ensures that rules which can't be parsed are ignored and cause an install
// warning.
TEST_P(SingleRulesetIndexingTest, InvalidJSONRules_StrongTypes) {
  {
    TestRule rule = CreateGenericRule();
    rule.id = 1;
    AddRule(rule);
  }

  {
    TestRule rule = CreateGenericRule();
    rule.id = 2;
    rule.action->type = std::string("invalid action");
    AddRule(rule);
  }

  {
    TestRule rule = CreateGenericRule();
    rule.id = 3;
    AddRule(rule);
  }

  {
    TestRule rule = CreateGenericRule();
    rule.id = 4;
    rule.condition->domain_type = std::string("invalid_domain_type");
    AddRule(rule);
  }

  extension_loader()->set_ignore_manifest_warnings(true);
  LoadAndExpectSuccess(2 /* rules count */);

  // TODO(crbug.com/879355): CrxInstaller reloads the extension after moving it,
  // which causes it to lose the install warning. This should be fixed.
  if (GetParam() != ExtensionLoadType::PACKED) {
    ASSERT_EQ(2u, extension()->install_warnings().size());
    std::vector<InstallWarning> expected_warnings;

    for (const auto& warning : extension()->install_warnings()) {
      EXPECT_EQ(extensions::manifest_keys::kDeclarativeNetRequestKey,
                warning.key);
      EXPECT_EQ(extensions::manifest_keys::kDeclarativeRuleResourcesKey,
                warning.specific);
      EXPECT_THAT(warning.message, ::testing::HasSubstr("Parse error"));
    }
  }
}

// Ensures that rules which can't be parsed are ignored and cause an install
// warning.
TEST_P(SingleRulesetIndexingTest, InvalidJSONRules_Parsed) {
  const char* kRules = R"(
    [
      {
        "id" : 1,
        "priority": 1,
        "condition" : [],
        "action" : {"type" : "block" }
      },
      {
        "id" : 2,
        "priority": 1,
        "condition" : {"urlFilter" : "abc"},
        "action" : {"type" : "block" }
      },
      {
        "id" : 3,
        "priority": 1,
        "invalidKey" : "invalidKeyValue",
        "condition" : {"urlFilter" : "example"},
        "action" : {"type" : "block" }
      },
      {
        "id" : "6",
        "priority": 1,
        "condition" : {"urlFilter" : "google"},
        "action" : {"type" : "block" }
      }
    ]
  )";
  SetRules(base::JSONReader::ReadDeprecated(kRules));

  extension_loader()->set_ignore_manifest_warnings(true);
  LoadAndExpectSuccess(1 /* rules count */);

  // TODO(crbug.com/879355): CrxInstaller reloads the extension after moving it,
  // which causes it to lose the install warning. This should be fixed.
  if (GetParam() != ExtensionLoadType::PACKED) {
    ASSERT_EQ(3u, extension()->install_warnings().size());
    std::vector<InstallWarning> expected_warnings;

    expected_warnings.emplace_back(
        ErrorUtils::FormatErrorMessage(
            GetErrorWithFilename(kRuleNotParsedWarning), "id 1",
            "'condition': expected dictionary, got list"),
        manifest_keys::kDeclarativeNetRequestKey,
        manifest_keys::kDeclarativeRuleResourcesKey);
    expected_warnings.emplace_back(
        ErrorUtils::FormatErrorMessage(
            GetErrorWithFilename(kRuleNotParsedWarning), "id 3",
            "found unexpected key 'invalidKey'"),
        manifest_keys::kDeclarativeNetRequestKey,
        manifest_keys::kDeclarativeRuleResourcesKey);
    expected_warnings.emplace_back(
        ErrorUtils::FormatErrorMessage(
            GetErrorWithFilename(kRuleNotParsedWarning), "index 4",
            "'id': expected id, got string"),
        manifest_keys::kDeclarativeNetRequestKey,
        manifest_keys::kDeclarativeRuleResourcesKey);
    EXPECT_EQ(expected_warnings, extension()->install_warnings());
  }
}

// Ensure that we can add up to MAX_NUMBER_OF_RULES.
TEST_P(SingleRulesetIndexingTest, RuleCountLimitMatched) {
  TestRule rule = CreateGenericRule();
  for (int i = 0; i < dnr_api::MAX_NUMBER_OF_RULES; ++i) {
    rule.id = kMinValidID + i;
    rule.condition->url_filter = std::to_string(i);
    AddRule(rule);
  }
  LoadAndExpectSuccess(dnr_api::MAX_NUMBER_OF_RULES);
}

// Ensure that we get an install warning on exceeding the rule count limit.
TEST_P(SingleRulesetIndexingTest, RuleCountLimitExceeded) {
  TestRule rule = CreateGenericRule();
  for (int i = 1; i <= dnr_api::MAX_NUMBER_OF_RULES + 1; ++i) {
    rule.id = kMinValidID + i;
    rule.condition->url_filter = std::to_string(i);
    AddRule(rule);
  }

  extension_loader()->set_ignore_manifest_warnings(true);
  LoadAndExpectSuccess(dnr_api::MAX_NUMBER_OF_RULES);

  // TODO(crbug.com/879355): CrxInstaller reloads the extension after moving it,
  // which causes it to lose the install warning. This should be fixed.
  if (GetParam() != ExtensionLoadType::PACKED) {
    ASSERT_EQ(1u, extension()->install_warnings().size());
    EXPECT_EQ(InstallWarning(GetErrorWithFilename(kRuleCountExceeded),
                             manifest_keys::kDeclarativeNetRequestKey,
                             manifest_keys::kDeclarativeRuleResourcesKey),
              extension()->install_warnings()[0]);
  }
}

// Ensure that regex rules which exceed the per rule memory limit are ignored
// and raise an install warning.
TEST_P(SingleRulesetIndexingTest, LargeRegexIgnored) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter.reset();
  int id = kMinValidID;

  const int kNumSmallRegex = 5;
  std::string small_regex = "http://(yahoo|google)\\.com";
  for (int i = 0; i < kNumSmallRegex; i++, id++) {
    rule.id = id;
    rule.condition->regex_filter = small_regex;
    AddRule(rule);
  }

  const int kNumLargeRegex = 2;
  for (int i = 0; i < kNumLargeRegex; i++, id++) {
    rule.id = id;
    rule.condition->regex_filter = kLargeRegexFilter;
    AddRule(rule);
  }

  base::HistogramTester tester;
  extension_loader()->set_ignore_manifest_warnings(true);

  LoadAndExpectSuccess(kNumSmallRegex);

  tester.ExpectBucketCount(kIsLargeRegexHistogram, true, kNumLargeRegex);
  tester.ExpectBucketCount(kIsLargeRegexHistogram, false, kNumSmallRegex);

  // TODO(crbug.com/879355): CrxInstaller reloads the extension after moving it,
  // which causes it to lose the install warning. This should be fixed.
  if (GetParam() != ExtensionLoadType::PACKED) {
    InstallWarning warning_1 = GetLargeRegexWarning(kMinValidID + 5);
    InstallWarning warning_2 = GetLargeRegexWarning(kMinValidID + 6);
    EXPECT_THAT(
        extension()->install_warnings(),
        ::testing::UnorderedElementsAre(::testing::Eq(std::cref(warning_1)),
                                        ::testing::Eq(std::cref(warning_2))));
  }
}

// Test an extension with both an error and an install warning.
TEST_P(SingleRulesetIndexingTest, WarningAndError) {
  // Add a large regex rule which will exceed the per rule memory limit and
  // cause an install warning.
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter.reset();
  rule.id = kMinValidID;
  rule.condition->regex_filter = kLargeRegexFilter;
  AddRule(rule);

  // Add a regex rule with a syntax error.
  rule.condition->regex_filter = "abc(";
  rule.id = kMinValidID + 1;
  AddRule(rule);

  extension_loader()->set_ignore_manifest_warnings(true);
  LoadAndExpectError(
      GetParseError(ParseResult::ERROR_INVALID_REGEX_FILTER, kMinValidID + 1));
}

// Ensure that we get an install warning on exceeding the regex rule count
// limit.
TEST_P(SingleRulesetIndexingTest, RegexRuleCountExceeded) {
  TestRule regex_rule = CreateGenericRule();
  regex_rule.condition->url_filter.reset();
  int rule_id = kMinValidID;
  for (int i = 1; i <= dnr_api::MAX_NUMBER_OF_REGEX_RULES + 5; ++i, ++rule_id) {
    regex_rule.id = rule_id;
    regex_rule.condition->regex_filter = std::to_string(i);
    AddRule(regex_rule);
  }

  const int kCountNonRegexRules = 5;
  TestRule rule = CreateGenericRule();
  for (int i = 1; i <= kCountNonRegexRules; i++, ++rule_id) {
    rule.id = rule_id;
    rule.condition->url_filter = std::to_string(i);
    AddRule(rule);
  }

  extension_loader()->set_ignore_manifest_warnings(true);
  LoadAndExpectSuccess(dnr_api::MAX_NUMBER_OF_REGEX_RULES +
                       kCountNonRegexRules);
  // TODO(crbug.com/879355): CrxInstaller reloads the extension after moving it,
  // which causes it to lose the install warning. This should be fixed.
  if (GetParam() != ExtensionLoadType::PACKED) {
    ASSERT_EQ(1u, extension()->install_warnings().size());
    EXPECT_EQ(InstallWarning(GetErrorWithFilename(kRegexRuleCountExceeded),
                             manifest_keys::kDeclarativeNetRequestKey,
                             manifest_keys::kDeclarativeRuleResourcesKey),
              extension()->install_warnings()[0]);
  }
}

TEST_P(SingleRulesetIndexingTest, InvalidJSONFile) {
  set_persist_invalid_json_file();
  // The error is returned by the JSON parser we use. Hence just test an error
  // is raised.
  LoadAndExpectError("");
}

TEST_P(SingleRulesetIndexingTest, EmptyRuleset) {
  LoadAndExpectSuccess(0 /* rules count */);
}

TEST_P(SingleRulesetIndexingTest, AddSingleRule) {
  AddRule(CreateGenericRule());
  LoadAndExpectSuccess(1 /* rules count */);
}

TEST_P(SingleRulesetIndexingTest, AddTwoRules) {
  TestRule rule = CreateGenericRule();
  AddRule(rule);

  rule.id = kMinValidID + 1;
  AddRule(rule);
  LoadAndExpectSuccess(2 /* rules count */);
}

// Test that we do not use an extension provided indexed ruleset.
TEST_P(SingleRulesetIndexingTest, ExtensionWithIndexedRuleset) {
  set_persist_initial_indexed_ruleset();
  AddRule(CreateGenericRule());
  LoadAndExpectSuccess(1 /* rules count */);
}

// Tests that multiple static rulesets are correctly indexed.
class MultipleRulesetsIndexingTest : public RuleIndexingTestBase {
 public:
  MultipleRulesetsIndexingTest() = default;

 protected:
  // RuleIndexingTestBase override:
  void WriteExtensionData() override {
    WriteManifestAndRulesets(extension_dir(), rulesets_, {} /* hosts */);
  }

  void AddRuleset(TestRulesetInfo info) {
    rulesets_.push_back(std::move(info));
  }

 private:
  std::vector<TestRulesetInfo> rulesets_;
};

// Tests an extension with multiple static rulesets.
TEST_P(MultipleRulesetsIndexingTest, Success) {
  size_t kNumRulesets = 7;
  size_t kRulesPerRuleset = 10;

  std::vector<TestRule> rules;
  for (size_t i = 0; i < kRulesPerRuleset; ++i) {
    TestRule rule = CreateGenericRule();
    rule.id = kMinValidID + i;
    rules.push_back(rule);
  }
  base::Value rules_value = std::move(*ToListValue(rules));

  for (size_t i = 0; i < kNumRulesets; ++i) {
    TestRulesetInfo info;
    info.relative_file_path = std::to_string(i);
    info.rules_value = rules_value.Clone();
    AddRuleset(std::move(info));
  }

  LoadAndExpectSuccess(kNumRulesets *
                       kRulesPerRuleset /* expected_indexed_rules_count */);
}

// Tests an extension with multiple empty rulesets.
TEST_P(MultipleRulesetsIndexingTest, EmptyRulesets) {
  size_t kNumRulesets = 7;

  for (size_t i = 0; i < kNumRulesets; ++i) {
    TestRulesetInfo info;
    info.relative_file_path = std::to_string(i);
    info.rules_value = base::ListValue();
    AddRuleset(std::move(info));
  }

  LoadAndExpectSuccess(0u /* expected_indexed_rules_count */);
}

// Tests an extension with multiple static rulesets, with one of rulesets
// specifying an invalid rules file.
TEST_P(MultipleRulesetsIndexingTest, ListNotPassed) {
  {
    std::vector<TestRule> rules({CreateGenericRule()});
    TestRulesetInfo info1;
    info1.relative_file_path = "1";
    info1.rules_value = std::move(*ToListValue(rules));
    AddRuleset(std::move(info1));
  }

  {
    // Persist a ruleset with an invalid rules file.
    TestRulesetInfo info2;
    info2.relative_file_path = "2";
    info2.rules_value = base::DictionaryValue();
    AddRuleset(std::move(info2));
  }

  {
    TestRulesetInfo info3;
    info3.relative_file_path = "3";
    info3.rules_value = base::ListValue();
    AddRuleset(std::move(info3));
  }

  LoadAndExpectError(kErrorListNotPassed, "2" /* filename */);
}

// Tests an extension with multiple static rulesets with each ruleset generating
// some install warnings.
TEST_P(MultipleRulesetsIndexingTest, InstallWarnings) {
  size_t expected_rule_count = 0;
  std::vector<std::string> expected_warnings;
  {
    // Persist a ruleset with an install warning for a large regex.
    std::vector<TestRule> rules;
    TestRule rule = CreateGenericRule();
    rule.id = kMinValidID;
    rules.push_back(rule);

    rule.id = kMinValidID + 1;
    rule.condition->url_filter.reset();
    rule.condition->regex_filter = kLargeRegexFilter;
    rules.push_back(rule);

    TestRulesetInfo info;
    info.relative_file_path = "1.json";
    info.rules_value = std::move(*ToListValue(rules));

    expected_warnings.push_back(
        GetLargeRegexWarning(*rule.id, info.relative_file_path).message);

    AddRuleset(std::move(info));

    expected_rule_count += rules.size();
  }

  {
    // Persist a ruleset with an install warning for exceeding the rule count.
    std::vector<TestRule> rules;
    for (int i = 1; i <= dnr_api::MAX_NUMBER_OF_RULES + 1; ++i) {
      TestRule rule = CreateGenericRule();
      rule.id = kMinValidID + i;
      rules.push_back(rule);
    }

    TestRulesetInfo info;
    info.relative_file_path = "2.json";
    info.rules_value = std::move(*ToListValue(rules));

    expected_warnings.push_back(
        GetErrorWithFilename(kRuleCountExceeded, info.relative_file_path));

    AddRuleset(std::move(info));

    expected_rule_count += dnr_api::MAX_NUMBER_OF_RULES;
  }

  {
    // Persist a ruleset with an install warning for exceeding the regex rule
    // count.
    std::vector<TestRule> rules;
    TestRule regex_rule = CreateGenericRule();
    regex_rule.condition->url_filter.reset();
    int rule_id = kMinValidID;
    for (int i = 1; i <= dnr_api::MAX_NUMBER_OF_REGEX_RULES + 1;
         ++i, ++rule_id) {
      regex_rule.id = rule_id;
      regex_rule.condition->regex_filter = std::to_string(i);
      rules.push_back(regex_rule);
    }

    const int kCountNonRegexRules = 5;
    TestRule rule = CreateGenericRule();
    for (int i = 1; i <= kCountNonRegexRules; i++, ++rule_id) {
      rule.id = rule_id;
      rule.condition->url_filter = std::to_string(i);
      rules.push_back(rule);
    }

    TestRulesetInfo info;
    info.relative_file_path = "3.json";
    info.rules_value = std::move(*ToListValue(rules));

    expected_warnings.push_back(
        GetErrorWithFilename(kRegexRuleCountExceeded, info.relative_file_path));

    AddRuleset(std::move(info));

    expected_rule_count +=
        kCountNonRegexRules + dnr_api::MAX_NUMBER_OF_REGEX_RULES;
  }

  extension_loader()->set_ignore_manifest_warnings(true);
  LoadAndExpectSuccess(expected_rule_count);

  // TODO(crbug.com/879355): CrxInstaller reloads the extension after moving it,
  // which causes it to lose the install warning. This should be fixed.
  if (GetParam() != ExtensionLoadType::PACKED) {
    const std::vector<InstallWarning>& warnings =
        extension()->install_warnings();
    std::vector<std::string> warning_strings;
    for (const InstallWarning& warning : warnings)
      warning_strings.push_back(warning.message);

    EXPECT_THAT(warning_strings,
                ::testing::UnorderedElementsAreArray(expected_warnings));
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         SingleRulesetIndexingTest,
                         ::testing::Values(ExtensionLoadType::PACKED,
                                           ExtensionLoadType::UNPACKED));
INSTANTIATE_TEST_SUITE_P(All,
                         MultipleRulesetsIndexingTest,
                         ::testing::Values(ExtensionLoadType::PACKED,
                                           ExtensionLoadType::UNPACKED));

}  // namespace
}  // namespace declarative_net_request
}  // namespace extensions
