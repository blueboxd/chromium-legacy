// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/version_info/version_info.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/dnr_manifest_data.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/value_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace keys = manifest_keys;
namespace errors = manifest_errors;
namespace dnr_api = api::declarative_net_request;

namespace declarative_net_request {
namespace {

constexpr char kJSONRulesFilename[] = "rules_file.json";
const base::FilePath::CharType kJSONRulesetFilepath[] =
    FILE_PATH_LITERAL("rules_file.json");

std::string GetRuleResourcesKey() {
  return base::JoinString(
      {keys::kDeclarativeNetRequestKey, keys::kDeclarativeRuleResourcesKey},
      ".");
}

// Fixture testing the kDeclarativeNetRequestKey manifest key.
class DNRManifestTest : public testing::Test {
 public:
  DNRManifestTest() : channel_(::version_info::Channel::UNKNOWN) {}

 protected:
  // Loads the extension and verifies the |expected_error|.
  void LoadAndExpectError(const std::string& expected_error) {
    std::string error;
    scoped_refptr<Extension> extension = file_util::LoadExtension(
        temp_dir_.GetPath(), Manifest::UNPACKED, Extension::NO_FLAGS, &error);
    EXPECT_FALSE(extension);
    EXPECT_EQ(expected_error, error);
  }

  // Loads the extension and verifies that the manifest info is correctly set
  // up.. Returns the loaded extension.
  void LoadAndExpectSuccess(const std::vector<base::FilePath>& expected_paths) {
    std::string error;
    scoped_refptr<Extension> extension = file_util::LoadExtension(
        temp_dir_.GetPath(), Manifest::UNPACKED, Extension::NO_FLAGS, &error);
    ASSERT_TRUE(extension) << error;
    EXPECT_TRUE(error.empty());

    ASSERT_TRUE(DNRManifestData::HasRuleset(*extension));

    const std::vector<DNRManifestData::RulesetInfo>& rulesets =
        DNRManifestData::GetRulesets(*extension);
    ASSERT_EQ(expected_paths.size(), rulesets.size());
    for (size_t i = 0; i < rulesets.size(); ++i) {
      EXPECT_GE(rulesets[i].id, kMinValidStaticRulesetID);
      EXPECT_EQ(rulesets[i].relative_path, expected_paths[i]);
    }
  }

  void WriteManifestAndRuleset(
      const base::Value& manifest,
      const std::vector<base::FilePath>& ruleset_paths) {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());

    for (const auto& path : ruleset_paths) {
      base::FilePath rules_path = temp_dir_.GetPath().Append(path);

      // Create parent directory of |rules_path| if it doesn't exist.
      EXPECT_TRUE(base::CreateDirectory(rules_path.DirName()));

      // Persist an empty ruleset file.
      EXPECT_EQ(0, base::WriteFile(rules_path, nullptr /*data*/, 0 /*size*/));
    }

    // Persist manifest file.
    JSONFileValueSerializer(temp_dir_.GetPath().Append(kManifestFilename))
        .Serialize(manifest);
  }

 private:
  base::ScopedTempDir temp_dir_;
  ScopedCurrentChannel channel_;

  DISALLOW_COPY_AND_ASSIGN(DNRManifestTest);
};

TEST_F(DNRManifestTest, EmptyRuleset) {
  base::FilePath ruleset_path = base::FilePath(kJSONRulesetFilepath);
  WriteManifestAndRuleset(*CreateManifest(kJSONRulesFilename), {ruleset_path});

  LoadAndExpectSuccess({ruleset_path});
}

TEST_F(DNRManifestTest, InvalidManifestKey) {
  std::unique_ptr<base::DictionaryValue> manifest =
      CreateManifest(kJSONRulesFilename);
  manifest->SetInteger(keys::kDeclarativeNetRequestKey, 3);
  WriteManifestAndRuleset(*manifest, {base::FilePath(kJSONRulesetFilepath)});
  LoadAndExpectError(
      ErrorUtils::FormatErrorMessage(errors::kInvalidDeclarativeNetRequestKey,
                                     keys::kDeclarativeNetRequestKey));
}

TEST_F(DNRManifestTest, InvalidRulesFileKey) {
  std::unique_ptr<base::DictionaryValue> manifest =
      CreateManifest(kJSONRulesFilename);
  manifest->SetInteger(GetRuleResourcesKey(), 3);
  WriteManifestAndRuleset(*manifest, {base::FilePath(kJSONRulesetFilepath)});
  LoadAndExpectError(ErrorUtils::FormatErrorMessage(
      errors::kInvalidDeclarativeRulesFileKey, keys::kDeclarativeNetRequestKey,
      keys::kDeclarativeRuleResourcesKey));
}

TEST_F(DNRManifestTest, MultipleRulesFileSuccess) {
  std::unique_ptr<base::DictionaryValue> manifest =
      CreateManifest(kJSONRulesFilename);

  ListBuilder rule_resources;

  base::FilePath path1(FILE_PATH_LITERAL("file1.json"));
  dnr_api::Ruleset ruleset;
  ruleset.path = path1.AsUTF8Unsafe();
  rule_resources.Append(ruleset.ToValue());

  base::FilePath path2(FILE_PATH_LITERAL("file2.json"));
  ruleset.path = path2.AsUTF8Unsafe();
  rule_resources.Append(ruleset.ToValue());

  base::FilePath path3(FILE_PATH_LITERAL("file3.json"));
  ruleset.path = path3.AsUTF8Unsafe();
  rule_resources.Append(ruleset.ToValue());

  manifest->SetList(GetRuleResourcesKey(), rule_resources.Build());

  std::vector<base::FilePath> paths = {path1, path2, path3};
  WriteManifestAndRuleset(*manifest, paths);

  LoadAndExpectSuccess(paths);
}

TEST_F(DNRManifestTest, MultipleRulesFileInvalidPath) {
  std::unique_ptr<base::DictionaryValue> manifest =
      CreateManifest(kJSONRulesFilename);

  ListBuilder rule_resources;

  base::FilePath path1(FILE_PATH_LITERAL("file1.json"));
  dnr_api::Ruleset ruleset;
  ruleset.path = path1.AsUTF8Unsafe();
  rule_resources.Append(ruleset.ToValue());

  base::FilePath path2(FILE_PATH_LITERAL("file2.json"));
  ruleset.path = path2.AsUTF8Unsafe();
  rule_resources.Append(ruleset.ToValue());

  manifest->SetList(GetRuleResourcesKey(), rule_resources.Build());

  // Only persist |path1|.
  WriteManifestAndRuleset(*manifest, {path1});

  LoadAndExpectError(ErrorUtils::FormatErrorMessage(
      errors::kRulesFileIsInvalid, keys::kDeclarativeNetRequestKey,
      keys::kDeclarativeRuleResourcesKey, path2.AsUTF8Unsafe()));
}

TEST_F(DNRManifestTest, RulesetCountExceeded) {
  std::unique_ptr<base::DictionaryValue> manifest =
      CreateManifest(kJSONRulesFilename);

  ListBuilder rule_resources;

  std::vector<base::FilePath> paths;
  for (int i = 0; i <= dnr_api::MAX_NUMBER_OF_STATIC_RULESETS; ++i) {
    base::FilePath path = base::FilePath().AppendASCII(base::NumberToString(i));
    paths.push_back(path);

    dnr_api::Ruleset ruleset;
    ruleset.path = path.AsUTF8Unsafe();

    rule_resources.Append(ruleset.ToValue());
  }

  manifest->SetList(GetRuleResourcesKey(), rule_resources.Build());

  WriteManifestAndRuleset(*manifest, paths);

  LoadAndExpectError(ErrorUtils::FormatErrorMessage(
      errors::kRulesetCountExceeded, keys::kDeclarativeNetRequestKey,
      keys::kDeclarativeRuleResourcesKey,
      base::NumberToString(dnr_api::MAX_NUMBER_OF_STATIC_RULESETS)));
}

TEST_F(DNRManifestTest, NonExistentRulesFile) {
  dnr_api::Ruleset ruleset;
  ruleset.path = "invalid_file.json";

  std::unique_ptr<base::DictionaryValue> manifest =
      CreateManifest(kJSONRulesFilename);
  manifest->SetList(GetRuleResourcesKey(),
                    ListBuilder().Append(ruleset.ToValue()).Build());

  WriteManifestAndRuleset(*manifest, {base::FilePath(kJSONRulesetFilepath)});

  LoadAndExpectError(ErrorUtils::FormatErrorMessage(
      errors::kRulesFileIsInvalid, keys::kDeclarativeNetRequestKey,
      keys::kDeclarativeRuleResourcesKey, ruleset.path));
}

TEST_F(DNRManifestTest, NeedsDeclarativeNetRequestPermission) {
  std::unique_ptr<base::DictionaryValue> manifest =
      CreateManifest(kJSONRulesFilename);
  // Remove "declarativeNetRequest" permission.
  manifest->Remove(keys::kPermissions, nullptr);

  WriteManifestAndRuleset(*manifest, {base::FilePath(kJSONRulesetFilepath)});

  LoadAndExpectError(ErrorUtils::FormatErrorMessage(
      errors::kDeclarativeNetRequestPermissionNeeded, kAPIPermission,
      keys::kDeclarativeNetRequestKey));
}

TEST_F(DNRManifestTest, RulesFileInNestedDirectory) {
  base::FilePath nested_path =
      base::FilePath(FILE_PATH_LITERAL("dir")).Append(kJSONRulesetFilepath);
  std::unique_ptr<base::DictionaryValue> manifest =
      CreateManifest(kJSONRulesFilename);

  dnr_api::Ruleset ruleset;
  ruleset.path = "dir/rules_file.json";

  manifest->SetList(GetRuleResourcesKey(),
                    ListBuilder().Append(ruleset.ToValue()).Build());

  WriteManifestAndRuleset(*manifest, {nested_path});

  LoadAndExpectSuccess({nested_path});
}

}  // namespace
}  // namespace declarative_net_request
}  // namespace extensions
