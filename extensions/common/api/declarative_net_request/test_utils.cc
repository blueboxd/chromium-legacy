// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/declarative_net_request/test_utils.h"

#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/value_builder.h"

namespace extensions {
namespace keys = manifest_keys;
namespace dnr_api = api::declarative_net_request;

namespace declarative_net_request {

namespace {

const base::FilePath::CharType kBackgroundScriptFilepath[] =
    FILE_PATH_LITERAL("background.js");

std::unique_ptr<base::Value> ToValue(const std::string& t) {
  return std::make_unique<base::Value>(t);
}

std::unique_ptr<base::Value> ToValue(int t) {
  return std::make_unique<base::Value>(t);
}

std::unique_ptr<base::Value> ToValue(bool t) {
  return std::make_unique<base::Value>(t);
}

std::unique_ptr<base::Value> ToValue(const DictionarySource& source) {
  return source.ToValue();
}

template <typename T>
std::unique_ptr<base::Value> ToValue(const std::vector<T>& vec) {
  ListBuilder builder;
  for (const T& t : vec)
    builder.Append(ToValue(t));
  return builder.Build();
}

template <typename T>
void SetValue(base::DictionaryValue* dict,
              const char* key,
              const base::Optional<T>& value) {
  if (!value)
    return;

  dict->Set(key, ToValue(*value));
}

// Helper to build an extension manifest which uses the
// kDeclarativeNetRequestKey manifest key. |hosts| specifies the host
// permissions to grant. |flags| is a bitmask of ConfigFlag to configure the
// extension. |ruleset_info| specifies the static rulesets for the extension.
std::unique_ptr<base::DictionaryValue> CreateManifest(
    const std::vector<TestRulesetInfo>& ruleset_info,
    const std::vector<std::string>& hosts,
    unsigned flags) {
  std::vector<std::string> permissions = hosts;
  permissions.push_back(kAPIPermission);

  // These permissions are needed for some tests. TODO(karandeepb): Add a
  // ConfigFlag for these.
  permissions.push_back("webRequest");
  permissions.push_back("webRequestBlocking");

  if (flags & kConfig_HasFeedbackPermission)
    permissions.push_back(kFeedbackAPIPermission);

  if (flags & kConfig_HasActiveTab)
    permissions.push_back("activeTab");

  std::vector<std::string> background_scripts;
  if (flags & kConfig_HasBackgroundScript)
    background_scripts.push_back("background.js");

  ListBuilder rule_resources_builder;
  for (const TestRulesetInfo& info : ruleset_info) {
    dnr_api::Ruleset ruleset;
    ruleset.path = info.relative_file_path;
    rule_resources_builder.Append(ruleset.ToValue());
  }

  return DictionaryBuilder()
      .Set(keys::kName, "Test extension")
      .Set(keys::kDeclarativeNetRequestKey,
           DictionaryBuilder()
               .Set(keys::kDeclarativeRuleResourcesKey,
                    rule_resources_builder.Build())
               .Build())
      .Set(keys::kPermissions, ToListValue(permissions))
      .Set(keys::kVersion, "1.0")
      .Set(keys::kManifestVersion, 2)
      .Set("background", DictionaryBuilder()
                             .Set("scripts", ToListValue(background_scripts))
                             .Build())
      .Set(keys::kBrowserAction, DictionaryBuilder().Build())
      .Build();
}

}  // namespace

TestRuleCondition::TestRuleCondition() = default;
TestRuleCondition::~TestRuleCondition() = default;
TestRuleCondition::TestRuleCondition(const TestRuleCondition&) = default;
TestRuleCondition& TestRuleCondition::operator=(const TestRuleCondition&) =
    default;

std::unique_ptr<base::DictionaryValue> TestRuleCondition::ToValue() const {
  auto dict = std::make_unique<base::DictionaryValue>();
  SetValue(dict.get(), kUrlFilterKey, url_filter);
  SetValue(dict.get(), kRegexFilterKey, regex_filter);
  SetValue(dict.get(), kIsUrlFilterCaseSensitiveKey,
           is_url_filter_case_sensitive);
  SetValue(dict.get(), kDomainsKey, domains);
  SetValue(dict.get(), kExcludedDomainsKey, excluded_domains);
  SetValue(dict.get(), kResourceTypesKey, resource_types);
  SetValue(dict.get(), kExcludedResourceTypesKey, excluded_resource_types);
  SetValue(dict.get(), kDomainTypeKey, domain_type);
  return dict;
}

TestRuleQueryKeyValue::TestRuleQueryKeyValue() = default;
TestRuleQueryKeyValue::~TestRuleQueryKeyValue() = default;
TestRuleQueryKeyValue::TestRuleQueryKeyValue(const TestRuleQueryKeyValue&) =
    default;
TestRuleQueryKeyValue& TestRuleQueryKeyValue::operator=(
    const TestRuleQueryKeyValue&) = default;

std::unique_ptr<base::DictionaryValue> TestRuleQueryKeyValue::ToValue() const {
  auto dict = std::make_unique<base::DictionaryValue>();
  SetValue(dict.get(), kQueryKeyKey, key);
  SetValue(dict.get(), kQueryValueKey, value);
  return dict;
}

TestRuleQueryTransform::TestRuleQueryTransform() = default;
TestRuleQueryTransform::~TestRuleQueryTransform() = default;
TestRuleQueryTransform::TestRuleQueryTransform(const TestRuleQueryTransform&) =
    default;
TestRuleQueryTransform& TestRuleQueryTransform::operator=(
    const TestRuleQueryTransform&) = default;

std::unique_ptr<base::DictionaryValue> TestRuleQueryTransform::ToValue() const {
  auto dict = std::make_unique<base::DictionaryValue>();
  SetValue(dict.get(), kQueryTransformRemoveParamsKey, remove_params);
  SetValue(dict.get(), kQueryTransformAddReplaceParamsKey,
           add_or_replace_params);
  return dict;
}

TestRuleTransform::TestRuleTransform() = default;
TestRuleTransform::~TestRuleTransform() = default;
TestRuleTransform::TestRuleTransform(const TestRuleTransform&) = default;
TestRuleTransform& TestRuleTransform::operator=(const TestRuleTransform&) =
    default;

std::unique_ptr<base::DictionaryValue> TestRuleTransform::ToValue() const {
  auto dict = std::make_unique<base::DictionaryValue>();
  SetValue(dict.get(), kTransformSchemeKey, scheme);
  SetValue(dict.get(), kTransformHostKey, host);
  SetValue(dict.get(), kTransformPortKey, port);
  SetValue(dict.get(), kTransformPathKey, path);
  SetValue(dict.get(), kTransformQueryKey, query);
  SetValue(dict.get(), kTransformQueryTransformKey, query_transform);
  SetValue(dict.get(), kTransformFragmentKey, fragment);
  SetValue(dict.get(), kTransformUsernameKey, username);
  SetValue(dict.get(), kTransformPasswordKey, password);
  return dict;
}

TestRuleRedirect::TestRuleRedirect() = default;
TestRuleRedirect::~TestRuleRedirect() = default;
TestRuleRedirect::TestRuleRedirect(const TestRuleRedirect&) = default;
TestRuleRedirect& TestRuleRedirect::operator=(const TestRuleRedirect&) =
    default;

std::unique_ptr<base::DictionaryValue> TestRuleRedirect::ToValue() const {
  auto dict = std::make_unique<base::DictionaryValue>();
  SetValue(dict.get(), kExtensionPathKey, extension_path);
  SetValue(dict.get(), kTransformKey, transform);
  SetValue(dict.get(), kRedirectUrlKey, url);
  SetValue(dict.get(), kRegexSubstitutionKey, regex_substitution);
  return dict;
}

TestRuleAction::TestRuleAction() = default;
TestRuleAction::~TestRuleAction() = default;
TestRuleAction::TestRuleAction(const TestRuleAction&) = default;
TestRuleAction& TestRuleAction::operator=(const TestRuleAction&) = default;

std::unique_ptr<base::DictionaryValue> TestRuleAction::ToValue() const {
  auto dict = std::make_unique<base::DictionaryValue>();
  SetValue(dict.get(), kRuleActionTypeKey, type);
  SetValue(dict.get(), kRemoveHeadersListKey, remove_headers_list);
  SetValue(dict.get(), kRedirectKey, redirect);
  return dict;
}

TestRule::TestRule() = default;
TestRule::~TestRule() = default;
TestRule::TestRule(const TestRule&) = default;
TestRule& TestRule::operator=(const TestRule&) = default;

std::unique_ptr<base::DictionaryValue> TestRule::ToValue() const {
  auto dict = std::make_unique<base::DictionaryValue>();
  SetValue(dict.get(), kIDKey, id);
  SetValue(dict.get(), kPriorityKey, priority);
  SetValue(dict.get(), kRuleConditionKey, condition);
  SetValue(dict.get(), kRuleActionKey, action);
  return dict;
}

TestRule CreateGenericRule() {
  TestRuleCondition condition;
  condition.url_filter = std::string("filter");
  TestRuleAction action;
  action.type = std::string("block");
  TestRule rule;
  rule.id = kMinValidID;
  rule.priority = kMinValidPriority;
  rule.action = action;
  rule.condition = condition;
  return rule;
}

std::unique_ptr<base::DictionaryValue> CreateManifest(
    const std::string& json_rules_filename,
    const std::vector<std::string>& hosts,
    unsigned flags) {
  std::vector<TestRulesetInfo> rulesets;
  rulesets.push_back({json_rules_filename, base::ListValue()});
  return CreateManifest(rulesets, hosts, flags);
}

std::unique_ptr<base::ListValue> ToListValue(
    const std::vector<std::string>& vec) {
  ListBuilder builder;
  for (const std::string& str : vec)
    builder.Append(str);
  return builder.Build();
}

std::unique_ptr<base::ListValue> ToListValue(const std::vector<TestRule>& vec) {
  ListBuilder builder;
  for (const TestRule& rule : vec)
    builder.Append(rule.ToValue());
  return builder.Build();
}

void WriteManifestAndRulesets(const base::FilePath& extension_dir,
                              const std::vector<TestRulesetInfo>& ruleset_info,
                              const std::vector<std::string>& hosts,
                              unsigned flags) {
  // Persist JSON rules files.
  for (const TestRulesetInfo& info : ruleset_info) {
    JSONFileValueSerializer(extension_dir.AppendASCII(info.relative_file_path))
        .Serialize(info.rules_value);
  }

  // Persists a background script if needed.
  if (flags & ConfigFlag::kConfig_HasBackgroundScript) {
    std::string content = "chrome.test.sendMessage('ready');";
    CHECK_EQ(static_cast<int>(content.length()),
             base::WriteFile(extension_dir.Append(kBackgroundScriptFilepath),
                             content.c_str(), content.length()));
  }

  // Persist manifest file.
  JSONFileValueSerializer(extension_dir.Append(kManifestFilename))
      .Serialize(*CreateManifest(ruleset_info, hosts, flags));
}

void WriteManifestAndRuleset(const base::FilePath& extension_dir,
                             const TestRulesetInfo& info,
                             const std::vector<std::string>& hosts,
                             unsigned flags) {
  std::vector<TestRulesetInfo> rulesets;
  rulesets.push_back({info.relative_file_path, info.rules_value.Clone()});
  WriteManifestAndRulesets(extension_dir, rulesets, hosts, flags);
}

}  // namespace declarative_net_request
}  // namespace extensions
